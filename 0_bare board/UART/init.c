#include<s3c24xx.h>

void disable_watch_dog(void);
void memsetup(void);
void copy_steppingstone_to_sdram(void);
void clock_init(void);

void disable_watch_dog(void)
{
	WTCON=0;
}

#define S3C2410_MPLL_200MHZ		((0x5c<<12)|(0x04<<4)|(0x00))
#define S3C2440_MPLL_200MHZ		((0x5c<<12)|(0x01<<4)|(0x02))

void clock_init(void)
{
	CLKDIVN=0x03;

__asm__(
		"mrc p15,0,r1,c1,c0,0\n"
		"orr r1,r1,#0xc0000000"
		"mcr p15,0,r1,c1,c0,0\n"
		);
		//判断是S3C2410 OR S3C2440  gstatus
	if((GSTATUS1==0x32410000)||(GSTATUS==0x32410002))
	{
		MPLLCON=S3C3410_MPLL_200MHZ;
	}
	else
	{
		MPLLCON=S3C2440_MPLL_200MHZ;
	}
}

void memsetup(void)
{
	volatile unsigned long *p=(volatile unsigned long *)MEM_CTL_BASE;
	//使用位置无关码形式  bl b   p[0]=xxxxx;
	p[0]=0x22011110;
	p[1]=0x00000700;
	p[2]=0x00000700;
	p[3]=0x00000700;
	p[4]=0x00000700;
	p[5]=0x00000700;
	p[6]=0x00000700;
	p[7]=0x00018005;
	p[8]=0x00018005;

	p[9]=0x008C04F4;
	p[10]=0x000000B1;
	p[11]=0x00000030;
	p[12]=0x00000030;
}

void copy_steppingstone_to_sdram(void)
{
	unsigned int *pdwSrc=(unsigned int *)0;
	unsigned int *pdwDest=(unsigned int *)0x30000000;

	while(pdwSrc<(unsigned int *)4096)
	{
		*pdwDest=*pdwSrc;
		pdwDest++;
		pdwSrc++;
	}
}