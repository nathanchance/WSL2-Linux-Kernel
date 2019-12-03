/* SPDX-License-Identifier: GPL-2.0 */
/*
 * With CONFIG_LTO_CLANG, LLD always enables -fdata-sections and
 * -ffunction-sections, which increases the size of the final module.
 * Merge the split sections in the final binary.
 */
SECTIONS {
	__patchable_function_entries : { *(__patchable_function_entries) }

	.bss : {
		*(.bss .bss.[0-9a-zA-Z_]*)
		*(.bss..L* .bss..compoundliteral*)
	}

	.data : {
		*(.data .data.[0-9a-zA-Z_]*)
		*(.data..L* .data..compoundliteral*)
	}

	.rodata : {
		*(.rodata .rodata.[0-9a-zA-Z_]*)
		*(.rodata..L* .rodata..compoundliteral*)
	}

	.text : { *(.text .text.[0-9a-zA-Z_]*) }
}
