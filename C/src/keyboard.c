#include "keyboard.h"

#include <stdint.h>

#include "io.h"
#include "pic.h"
#include "shell.h"
#include "string.h"
#include "vga.h"

// =====================================================================
// Keyboard driver (IRQ1)
//
// The keyboard controller hands us "scancodes" (set 1), not ASCII: one
// byte per key going down, and the same byte with the high bit set
// (+0x80) when it comes back up. "Extended" keys (the arrow cluster,
// Home/End/Delete, the right-hand Ctrl/Alt, ...) are prefixed with an
// extra 0xE0 byte first, because the original 1980s scancode set ran
// out of single-byte codes.
//
// This file turns that raw stream into ASCII, tracks modifier keys
// (shift/ctrl/alt/caps), and maintains an editable line of input for
// the shell: a real cursor position separate from the line's length,
// so arrows/Home/End/Delete can move around and edit anywhere in the
// line, not just delete off the end.
// =====================================================================

// left-hand modifiers are plain, single-byte scancodes. The right-hand
// Ctrl/Alt are "extended" (0xE0-prefixed) and share these exact same
// byte values once the 0xE0 has been consumed — see the `extended`
// handling in irq1_handler, which is where right ctrl/alt actually get
// recognized.
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_UP 0xAA
#define SC_RSHIFT_UP 0xB6
#define SC_CAPSLOCK 0x3A
#define SC_LCTRL 0x1D
#define SC_LCTRL_UP 0x9D
#define SC_LALT 0x38
#define SC_LALT_UP 0xB8

static unsigned char shift_pressed = 0;
static unsigned char caps_lock_on = 0;
static unsigned char ctrl_pressed = 0;
static unsigned char alt_pressed = 0;

// scancode set 1 -> ASCII, indexed by the raw byte read from the keyboard
// controller. 0 means "no ASCII equivalent" (shift, ctrl, arrow keys, etc).
// Letters are always lowercase here; case is decided later from shift and
// caps lock together, since caps lock (unlike shift) only affects letters.
// clang-format off
static const char scancode_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t',   'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,       'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\',  'z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};

static const char scancode_ascii_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t',   'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n',
    0,       'A','S','D','F','G','H','J','K','L',':','\"','~',
    0, '|',   'Z','X','C','V','B','N','M', '<', '>', '?', 0,
    '*', 0, ' ',
};
// clang-format on

#define TAB_WIDTH 4
#define INPUT_BUFFER_SIZE 128
static char input_buffer[INPUT_BUFFER_SIZE];
static int input_length = 0; // how many bytes of input_buffer are valid
static int input_cursor = 0; // where the next insert/delete happens, 0..input_length

// screen position where the current line's input starts (right after the
// prompt), so the line can be redrawn from any edit point
static int line_start_row = 0;
static int line_start_col = 0;

static void capture_line_start(void) {
	vga_get_cursor(&line_start_row, &line_start_col);
}

// redraw input_buffer[pos..input_length) in place, then blank `trailing`
// leftover columns after it (for when editing made the line shorter than
// what's already on screen), and finally put the cursor back where editing
// left off. Every edit that changes the buffer's contents goes through
// this instead of hand-rolling its own screen updates.
static void redraw_from(int pos, int trailing) {
	vga_place_cursor(line_start_row, line_start_col, pos);
	for (int i = pos; i < input_length; i++) {
		vga_putc(input_buffer[i]);
	}
	for (int i = 0; i < trailing; i++) {
		vga_putc(' ');
	}
	vga_place_cursor(line_start_row, line_start_col, input_cursor);
}

static void move_cursor_to(int pos) {
	if (pos < 0) {
		pos = 0;
	}
	if (pos > input_length) {
		pos = input_length;
	}
	input_cursor = pos;
	vga_place_cursor(line_start_row, line_start_col, input_cursor);
}

// insert one character at the cursor, shifting everything after it right
static void insert_char(char c) {
	if (input_length >= INPUT_BUFFER_SIZE - 1) {
		return; // line's full, drop the keystroke
	}
	for (int i = input_length; i > input_cursor; i--) {
		input_buffer[i] = input_buffer[i - 1];
	}
	input_buffer[input_cursor] = c;
	input_length++;
	input_cursor++;
	redraw_from(input_cursor - 1, 0);
}

// remove the character just before the cursor (regular backspace)
static void backspace(void) {
	if (input_cursor == 0) {
		return;
	}
	for (int i = input_cursor - 1; i < input_length - 1; i++) {
		input_buffer[i] = input_buffer[i + 1];
	}
	input_length--;
	input_cursor--;
	redraw_from(input_cursor, 1);
}

// remove the character under the cursor (forward delete)
static void delete_forward(void) {
	if (input_cursor >= input_length) {
		return;
	}
	for (int i = input_cursor; i < input_length - 1; i++) {
		input_buffer[i] = input_buffer[i + 1];
	}
	input_length--;
	redraw_from(input_cursor, 1);
}

// Ctrl+U: wipe the whole line, wherever the cursor was
static void kill_line(void) {
	int old_length = input_length;
	input_length = 0;
	input_cursor = 0;
	redraw_from(0, old_length);
}

// Ctrl+W: delete the word behind the cursor (trailing spaces, then the
// run of non-space characters before them)
static void delete_word(void) {
	int end = input_cursor;
	int pos = input_cursor;
	while (pos > 0 && input_buffer[pos - 1] == ' ') {
		pos--;
	}
	while (pos > 0 && input_buffer[pos - 1] != ' ') {
		pos--;
	}
	int removed = end - pos;
	if (removed == 0) {
		return;
	}
	for (int i = pos; i + removed < input_length; i++) {
		input_buffer[i] = input_buffer[i + removed];
	}
	input_length -= removed;
	input_cursor = pos;
	redraw_from(pos, removed);
}

// Ctrl+L: clear the screen, then redraw the prompt and whatever's typed
// so far so mid-line editing isn't lost
static void clear_screen_and_redraw(void) {
	clear_vga();
	shell_prompt();
	capture_line_start();
	redraw_from(0, 0);
}

static void submit_line(void) {
	input_buffer[input_length] = 0;
	vga_putc('\n');
	shell_dispatch(input_buffer);
	input_length = 0;
	input_cursor = 0;
	shell_prompt();
	capture_line_start();
}

// Ctrl+C: abandon the line currently being typed
static void cancel_line(void) {
	input_length = 0;
	input_cursor = 0;
	vga_puts("^c\n");
	shell_prompt();
	capture_line_start();
}

// vector 33 (IRQ1): keyboard
void irq1_handler(void) {
	uint8_t scancode = inb(0x60);
	static int extended = 0;

	// 0xE0 announces that the next byte is an extended scancode
	if (scancode == 0xE0) {
		extended = 1;
		pic_send_eoi(1);
		return;
	}

	if (extended) {
		extended = 0;
		// releases repeat the same code with the high bit set; masking
		// it off lets press and release share one switch below
		uint8_t code = scancode & 0x7F;
		int release = scancode & 0x80;

		switch (code) {
		case 0x1D: // right ctrl
			ctrl_pressed = !release;
			break;
		case 0x38: // right alt
			alt_pressed = !release;
			break;
		case 0x4B: // left arrow
			if (!release) {
				move_cursor_to(input_cursor - 1);
			}
			break;
		case 0x4D: // right arrow
			if (!release) {
				move_cursor_to(input_cursor + 1);
			}
			break;
		case 0x47: // home
			if (!release) {
				move_cursor_to(0);
			}
			break;
		case 0x4F: // end
			if (!release) {
				move_cursor_to(input_length);
			}
			break;
		case 0x53: // delete
			if (!release) {
				delete_forward();
			}
			break;
		case 0x1C: // numpad enter — behaves the same as the main Enter key
			if (!release) {
				submit_line();
			}
			break;
		default:
			// up/down arrows (no command history to scroll through yet),
			// page up/down, insert, the GUI/menu keys, numpad '/': nothing
			// to do for any of these, so just ignore them
			break;
		}

		vga_sync_hw_cursor();
		pic_send_eoi(1);
		return;
	}

	// modifier keys
	switch (scancode) {
	case SC_LSHIFT:
	case SC_RSHIFT:
		shift_pressed = 1;
		pic_send_eoi(1);
		return;
	case SC_LSHIFT_UP:
	case SC_RSHIFT_UP:
		shift_pressed = 0;
		pic_send_eoi(1);
		return;
	case SC_CAPSLOCK:
		caps_lock_on = !caps_lock_on;
		pic_send_eoi(1);
		return;
	case SC_LCTRL:
		ctrl_pressed = 1;
		pic_send_eoi(1);
		return;
	case SC_LCTRL_UP:
		ctrl_pressed = 0;
		pic_send_eoi(1);
		return;
	case SC_LALT:
		alt_pressed = 1;
		pic_send_eoi(1);
		return;
	case SC_LALT_UP:
		alt_pressed = 0;
		pic_send_eoi(1);
		return;
	default:
		break;
	}

	// everything below is a plain (non-extended) key press or release
	if (!(scancode & 0x80)) { // press, not release
		char base = scancode_ascii[scancode];
		if (base) {
			char c;
			if (base >= 'a' && base <= 'z') {
				// caps lock and shift both flip letter case; holding
				// both cancels back out to lowercase, same as real hardware
				int upper = shift_pressed ^ caps_lock_on;
				c = upper ? (char)(base - ('a' - 'A')) : base;
			} else {
				c = shift_pressed ? scancode_ascii_shift[scancode] : base;
			}

			if (ctrl_pressed) {
				switch (clower(c)) {
				case 'c':
					cancel_line();
					break;
				case 'u':
					kill_line();
					break;
				case 'w':
					delete_word();
					break;
				case 'l':
					clear_screen_and_redraw();
					break;
				default:
					// no shortcut bound to this combination — swallow it
					// rather than typing a letter the user didn't mean to
					break;
				}
			} else if (alt_pressed) {
				// no Alt shortcuts defined yet; swallow rather than insert
			} else {
				if (input_length == 0 && input_cursor == 0) {
					capture_line_start();
				}
				switch (c) {
				case '\n':
					submit_line();
					break;
				case '\b':
					backspace();
					break;
				case '\t':
					for (int i = 0; i < TAB_WIDTH; i++) {
						insert_char(' ');
					}
					break;
				default:
					insert_char(c);
					break;
				}
			}
			vga_sync_hw_cursor();
		}
	}

	pic_send_eoi(1);
}
