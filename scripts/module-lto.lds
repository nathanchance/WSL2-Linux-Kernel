/* SPDX-License-Identifier: GPL-2.0 */
/*
 * With CONFIG_LTO_CLANG, LLD always enables -fdata-sections and
 * -ffunction-sections, which increases the size of the final module.
 * Merge the split sections in the final binary.
 */
SECTIONS {
	/*
	 * LLD may emit .eh_frame with CONFIG_CFI_CLANG despite
	 * -fno-asynchronous-unwind-tables. Discard the section.
	 */
	/DISCARD/ : {
		*(.eh_frame)
	}

	__patchable_function_entries : { *(__patchable_function_entries) }
	.bss : { *(.bss .bss.[0-9a-zA-Z_]* .bss..compoundliteral*) }
	.data : { *(.data .data.[0-9a-zA-Z_]* .data..L* .data..compoundliteral*) }
	.rela.data : { *(.rela.data .rela.data.[0-9a-zA-Z_]* .rela.data..L*) }
	.rela.rodata : { *(.rela.rodata .rela.rodata.[0-9a-zA-Z_]* .rela.rodata..L*) }
	.rela.text : { *(.rela.text .rela.text.[0-9a-zA-Z_]*) }
	.rodata : { *(.rodata .rodata.[0-9a-zA-Z_]* .rodata..L*) }
	.text : { *(.text .text.[0-9a-zA-Z_]*) }
}
