.extern main
.text
.global _start
_start:
Reset:
	ldr sp,=4096
	bl diable_watch_dog    @设置栈指针，以下都是C函数，调用前需要设置好栈
	bl clock_init
	bl memsetup
	bl copy_steppingstone_to_sdram
	ldr pc,=on_sdram
on_sdram:
	ldr sp,=0x34000000
	ldr lr,=halt_loop
	ldr pc,=main
halt_loop:
	b halt_loop