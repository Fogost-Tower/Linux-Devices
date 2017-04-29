.text
.global _start
_start:
	ldr		sp, =4096
	bl		disable_watch_dog
	bl 		uart_init
	mov 	r0, #0x6D
	bl 		putc
	bl		memsetup
	mov 	r0, #0x6E
	bl 		putc
	bl 		nand_init
	mov 	r0, #0x6F
	bl 		putc
	
	ldr 	r0,	=0x30000000
	mov 	r1, #0
	mov 	r2, #4096
	bl 		nand_read
	
	ldr		sp, =0x34000000
	ldr 	lr, =halt_loop
	b	 	pc, =main
	
halt_loop:
	b		halt_loop