#include "s3c24xx.h"

#define WTCON			(*(volatile unsigned long *)0x53000000)

#define MEM_CTL_BASE	0x48000000

#define TXD0READY	(1<<2)
#define RXD0READY	(1)

#define PLCK			12000000        //在head.s里面还没有初始化时钟， 所以为12000000
#define UART_CLK	PLCK
#define UART_BAUD_RATE	57600  //因为时钟只有12000000 所以 -  -最高波特率只能57600
#define UART_BRD	((UART_CLK) / (UART_BAUD_RATE * 16) - 1) 

void disable_watch_dog();
void memsetup();

void uart_init(void);
void putc(char c);
void puts(char *str);
void puthex(unsigned long val);

void disable_watch_dog()
{
	WTCON = 0;
}

void memsetup()
{
	int i=0;
	unsigned long *p = (unsigned long *)MEM_CTL_BASE;

#if 0  /*位置相关码*/
	unsigned long const mem_cfg_val[]={
		0x22011110,     //BWSCON
		0x00000700,     //BANKCON0
		0x00000700,     //BANKCON1
		0x00000700,     //BANKCON2
		0x00000700,     //BANKCON3  
		0x00000700,     //BANKCON4
		0x00000700,     //BANKCON5
		0x00018005,     //BANKCON6
		0x00018005,     //BANKCON7
		0x008C07A3,     //REFRESH
		0x000000B1,     //BANKSIZE
		0x00000030,     //MRSRB6
		0x00000030,     //MRSRB7
	};

 

	for( ; i<13; i++){
		p[i] = mem_cfg_val[i];
	}
#endif

	/*位置无关码：内存没初始化时，必须用位置无关码*/
	p[0] = 0x22011110;     //BWSCON
	p[1] = 0x00000700;     //BANKCON0
	p[2] = 0x00000700;     //BANKCON1
	p[3] = 0x00000700;     //BANKCON2
	p[4] = 0x00000700;     //BANKCON3  
	p[5] = 0x00000700;     //BANKCON4
	p[6] = 0x00000700;     //BANKCON5
	p[7] = 0x00018005;     //BANKCON6
	p[8] = 0x00018005;     //BANKCON7
	p[9] = 0x008C07A3;     //REFRESH
	p[10] = 0x000000B1;     //BANKSIZE
	p[11] = 0x00000030;     //MRSRB6
	p[12] = 0x00000030;     //MRSRB7 

}

void uart_init(void)
{
	GPHCON |= 0xa0;
	GPHUP	= 0x0c;

	ULCON0	= 0x03;
	UCON0	= 0x05;
	UFCON0	= 0x00;
	UMCON0 	= 0x00;
	UBRDIV0 = UART_BRD;
}

void putc(char c)
{
	while(!(UTRSTAT0 & TXD0READY));

	UTXH0 = c;
}

void puts(char *str)
{
	int i=0;
	while(str[i]){
		putc(str[i]);
		i++;
	}
}

void puthex(unsigned long val)
{
	/*val = 0x1234ABCD*/
	unsigned char c;

	int i;

	putc('0');
	putc('x');

	for(i=0; i<8; i++){
		c=(val >> ((7-i)*4)) & 0xF;
		if((c >= 0) && (c <= 9)){
			c = '0' + c;
		}
		else if((c >= 0xA) && (c <=0xF)){
			c = 'A' + (c - 0xA);
		}
		putc(c);
	}
}