#define GPFCON      (*(volatile unsigned long *)0x56000050)
#define GPFDAT      (*(volatile unsigned long *)0x56000054)

void wait(volatile unsigned long dly)
{
	for(;dly>0;dly--);
}

int main()
{
	unsigned long i=0;
    GPFCON = 0x00001500;    // ����GPF4Ϊ�����, λ[9:8]=0b01
    GPFDAT = 0x00000000;    // GPF4���0��LED1����
    
    while(1)
	{
		wait(30000);
		GPFDAT=(~(i<<4));
		if(++i==8)
		 i=0;
	}

    return 0;
}
