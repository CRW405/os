#define EFI_SUCCESS 0

typedef unsigned short Char16;
typedef void *EFI_handle; // pointer to firmware object
typedef unsigned long long EFI_status;

struct EFI_simple_text_output_protocol;

typedef EFI_status (*EFI_text_string)(struct EFI_simple_text_output_protocol *this, Char16 *string);

typedef struct EFI_simple_text_output_protocol {
	void *dummy;
	EFI_text_string output_string;
} EFI_simple_text_output_protocol;

typedef struct {
	char dummy[60];
	EFI_simple_text_output_protocol *con_out;
} EFI_system_table;

EFI_status __attribute__((ms_abi)) efi_main(EFI_handle Image_handle, EFI_system_table *System_table) {
	System_table->con_out->output_string(System_table->con_out, (Char16 *)L"Hello, World!\r\n");
	while (1) {
		__asm__ __volatile__("hlt");
	}
	return EFI_SUCCESS;
}
