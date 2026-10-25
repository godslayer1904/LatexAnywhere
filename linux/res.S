/* 把引擎字节码和图标嵌进程序（只读段，按需换页，不占私有内存） */
	.section .rodata
	.balign 16
	.global la_engine_start, la_engine_end, la_math_start, la_math_end, la_icon_start, la_icon_end
la_engine_start:
	.incbin LA_ENGINE_QBC
la_engine_end:
	.balign 16
la_math_start:
	.incbin LA_MATH_QBC
la_math_end:
	.balign 16
la_icon_start:
	.incbin LA_ICON_SVG
la_icon_end:
	.section .note.GNU-stack,"",@progbits
