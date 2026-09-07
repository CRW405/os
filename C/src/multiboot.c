#include "multiboot.h"

#include <stdint.h>

#include "string.h"
#include "vga.h"

// =====================================================================
// multiboot parsing
// =====================================================================

typedef struct multiboot2_tag {
	uint32_t type;
	uint32_t size;
} multiboot2_tag_t;

// tag type 4: basic memory info, reported by GRUB in kilobytes
typedef struct multiboot2_tag_basic_meminfo {
	uint32_t type;
	uint32_t size;
	uint32_t mem_lower;
	uint32_t mem_upper;
} multiboot2_tag_basic_meminfo_t;

// tag type 6: memory map, one entry per contiguous region of physical memory
typedef struct multiboot2_mmap_entry {
	uint64_t addr;
	uint64_t len;
	uint32_t type; // 1 = available, 3 = ACPI reclaimable, 4 = ACPI NVS, 5 = defective, else reserved
	uint32_t reserved;
} multiboot2_mmap_entry_t;

typedef struct multiboot2_tag_mmap {
	uint32_t type;
	uint32_t size;
	uint32_t entry_size;
	uint32_t entry_version;
	multiboot2_mmap_entry_t entries[]; // size / entry_size entries follow
} multiboot2_tag_mmap_t;

// tag type 5: which BIOS disk GRUB booted from
typedef struct multiboot2_tag_bootdev {
	uint32_t type;
	uint32_t size;
	uint32_t biosdev;
	uint32_t partition;
	uint32_t sub_partition;
} multiboot2_tag_bootdev_t;

// tag type 8: framebuffer info; color info fields follow but depend on
// framebuffer_type, so we only pull out the geometry here
typedef struct multiboot2_tag_framebuffer {
	uint32_t type;
	uint32_t size;
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type; // 0 = indexed, 1 = RGB, 2 = EGA text
	uint16_t reserved;
} multiboot2_tag_framebuffer_t;

// tag type 9: ELF section headers of the kernel image; entries follow this header
typedef struct multiboot2_tag_elf_sections {
	uint32_t type;
	uint32_t size;
	uint32_t num;
	uint32_t entsize;
	uint32_t shndx;
} multiboot2_tag_elf_sections_t;

// tag type 10: APM (Advanced Power Management) BIOS table
typedef struct multiboot2_tag_apm {
	uint32_t type;
	uint32_t size;
	uint16_t version;
	uint16_t cseg;
	uint32_t offset;
	uint16_t cseg_16;
	uint16_t dseg;
	uint16_t flags;
	uint16_t cseg_len;
	uint16_t cseg_16_len;
	uint16_t dseg_len;
} multiboot2_tag_apm_t;

// tag type 14: a copy of the ACPI 1.0 RSDP GRUB found
typedef struct multiboot2_tag_old_acpi {
	uint32_t type;
	uint32_t size;
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
} multiboot2_tag_old_acpi_t;

// tag type 21: the physical address the kernel image was actually loaded at
typedef struct multiboot2_tag_load_base_addr {
	uint32_t type;
	uint32_t size;
	uint32_t load_base_addr;
} multiboot2_tag_load_base_addr_t;

// saved by boot64.s at boot time, before %ebx gets reused for anything else
extern uint32_t multiboot_info;

static const char *mmap_entry_type_name(uint32_t type) {
	switch (type) {
	case 1:
		return "available";
	case 3:
		return "ACPI reclaimable";
	case 4:
		return "ACPI NVS";
	case 5:
		return "defective";
	default:
		return "reserved";
	}
}

static void parse_multiboot2_info(uint32_t addr) {
	if (addr & 7) {
		vga_puts("Error: multiboot2 info address is not 8-byte aligned\n");
		return;
	}

	uint32_t total_size = *(uint32_t *)addr;

	multiboot2_tag_t *tag = (multiboot2_tag_t *)(addr + 8);
	while (tag->type != 0) { // type 0 is the end tag
		switch (tag->type) {
		case 1: // boot command line
			vga_puts("Boot command line: ");
			vga_puts((const char *)tag + sizeof(multiboot2_tag_t));
			vga_putc('\n');
			break;
		case 2: // boot loader name
			vga_puts("Boot loader name: ");
			vga_puts((const char *)tag + sizeof(multiboot2_tag_t));
			vga_putc('\n');
			break;
		case 4: { // basic memory info
			multiboot2_tag_basic_meminfo_t *meminfo = (multiboot2_tag_basic_meminfo_t *)tag;
			char buffer[12];
			vga_puts("Lower memory: ");
			vga_puts(itoa(meminfo->mem_lower, buffer, 10));
			vga_puts(" KB, upper memory: ");
			vga_puts(itoa(meminfo->mem_upper, buffer, 10));
			vga_puts(" KB\n");
			break;
		}
		case 6: { // memory map
			multiboot2_tag_mmap_t *mmap = (multiboot2_tag_mmap_t *)tag;
			// entries are entry_size bytes apart, not necessarily sizeof(multiboot2_mmap_entry_t),
			// so walk by raw byte offset instead of indexing the flexible array directly
			uint8_t *entry_ptr = (uint8_t *)mmap->entries;
			uint8_t *entries_end = (uint8_t *)mmap + mmap->size;
			vga_puts("Memory map:\n");
			while (entry_ptr < entries_end) {
				multiboot2_mmap_entry_t *entry = (multiboot2_mmap_entry_t *)entry_ptr;
				vga_puts("  base=");
				vga_put_hex64(entry->addr); // vga_put_hex64 already prints the "0x" prefix
				vga_puts(" len=");
				vga_put_hex64(entry->len);
				vga_puts(" type=");
				vga_puts(mmap_entry_type_name(entry->type));
				vga_putc('\n');
				entry_ptr += mmap->entry_size;
			}
			break;
		}
		case 5: { // BIOS boot device
			multiboot2_tag_bootdev_t *bootdev = (multiboot2_tag_bootdev_t *)tag;
			char buffer[12];
			vga_puts("BIOS boot device: biosdev=0x");
			vga_puts(itoa(bootdev->biosdev, buffer, 16));
			vga_puts(" partition=");
			vga_puts(itoa(bootdev->partition, buffer, 10));
			vga_puts(" sub_partition=");
			vga_puts(itoa(bootdev->sub_partition, buffer, 10));
			vga_putc('\n');
			break;
		}
		case 8: { // framebuffer info
			multiboot2_tag_framebuffer_t *fb = (multiboot2_tag_framebuffer_t *)tag;
			char buffer[12];
			vga_puts("Framebuffer: ");
			vga_put_hex64(fb->framebuffer_addr);
			vga_puts(" ");
			vga_puts(itoa(fb->framebuffer_width, buffer, 10));
			vga_puts("x");
			vga_puts(itoa(fb->framebuffer_height, buffer, 10));
			vga_puts("x");
			vga_puts(itoa(fb->framebuffer_bpp, buffer, 10));
			vga_puts(" bpp, type=");
			vga_puts(itoa(fb->framebuffer_type, buffer, 10));
			vga_putc('\n');
			break;
		}
		case 9: { // ELF section headers of the kernel image
			multiboot2_tag_elf_sections_t *elf = (multiboot2_tag_elf_sections_t *)tag;
			char buffer[12];
			vga_puts("ELF sections: num=");
			vga_puts(itoa(elf->num, buffer, 10));
			vga_puts(" entsize=");
			vga_puts(itoa(elf->entsize, buffer, 10));
			vga_puts(" shndx=");
			vga_puts(itoa(elf->shndx, buffer, 10));
			vga_putc('\n');
			break;
		}
		case 10: { // APM BIOS table
			multiboot2_tag_apm_t *apm = (multiboot2_tag_apm_t *)tag;
			char buffer[12];
			vga_puts("APM table: version=0x");
			vga_puts(itoa(apm->version, buffer, 16));
			vga_puts(" cseg=0x");
			vga_puts(itoa(apm->cseg, buffer, 16));
			vga_puts(" offset=");
			vga_put_hex64(apm->offset); // a 32-bit value, so go through vga_put_hex64 rather than
			                            // itoa's signed int, in case the high bit is set
			vga_putc('\n');
			break;
		}
		case 14: { // ACPI old RSDP
			multiboot2_tag_old_acpi_t *acpi = (multiboot2_tag_old_acpi_t *)tag;
			vga_puts("ACPI old RSDP: rsdt_address=");
			vga_put_hex64(acpi->rsdt_address); // a physical address, so avoid itoa's signed int
			vga_putc('\n');
			break;
		}
		case 21: { // physical address the kernel image was loaded at
			multiboot2_tag_load_base_addr_t *load = (multiboot2_tag_load_base_addr_t *)tag;
			vga_puts("Image load base address: ");
			vga_put_hex64(load->load_base_addr);
			vga_putc('\n');
			break;
		}
		// Add more cases here for other tag types as needed
		default:
			vga_puts("Unknown tag type: ");
			int tag_type = tag->type;
			char buffer[12];
			itoa(tag_type, buffer, 10);
			vga_puts(buffer);
			vga_putc('\n');
			break;
		}
		uint32_t size = (tag->size + 7) & ~7; // align to 8 bytes
		tag = (multiboot2_tag_t *)((uint8_t *)tag + size);
	}
}

void multiboot_print_tags(void) {
	parse_multiboot2_info(multiboot_info);
}
