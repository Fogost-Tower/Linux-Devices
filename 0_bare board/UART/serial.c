#include "s3c24xx.h"
#include "serial.h"

#define TXD0READY	(1<<2)
#define RXD0READT	(1)

#define PCLK		50000000
#define UART_CLK	PLCK
#define UART_BAUD_RATE	115200
#define UART_BRD	((UART_CLK / (UART_BAUD_RATE * 16))-1)

void uarto_init(void)
{
	GPHCON |=0xa0;
	GPHUP=0x0c;
	
	ULCON0=0x03;
	UCON0=0x05;
	UFCON0=0x00;
	UMCON0=0x00;
	UBRDIV0=UART_BRD;
}

void putc(unsigned char c)
{
	while(!(UTRSTAT0 & TXD0READY));

	UTXH0=c;
}

unsigned char getc(void)
{
	while(!(UTRSTAT0 & RXD0READY));

	return URXH0;
}

int isDigit(unsigned char c)
{
	if(c >= '0' && c <= '9')
		return 1;
	else
		return 0;
}

int isLetter(unsigned char c)
{
	if(c >= 'a' && c <= 'z')
		return 1;
	else if(c>='A' && c<='Z')
		return 1;
	else
		return 0;
}