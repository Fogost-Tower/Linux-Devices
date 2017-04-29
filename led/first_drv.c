#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define ulong unsigned long
#define uchar unsigned char
#define uint  unsigned int


#define DEV_NAME	"led"   //设备的名字
#define DEV_MAJOR	252     //设备的设备主设备号

static struct class *leds_class;					//定义设备类
static struct class_device *leds_class_device[4];      //定义设备驱动类

/***********定义通用端口的物理地址******************/
static ulong GPIO_VA;        //定义通用端口的虚拟地址
#define GPIO_OFF(x) ((x)-0x56000000)

#define GPFCON (*(volatile ulong *))(GPIO_VA+GPIO_OFF(0x56000050))
#define GPFDAT (*(volatile ulong *))(GPIO_VA+GPIO_OFF(0x56000054))


static int S3C2440_LEDS_OPEN(struct inode *inode,struct file *file)
{
	int minor = MINOR(inode->i_dev);

	switch(minor)
	{
		case 0:
		{
			GPFCON &= ~((0x3<<(4*2)) | (0x3<<(5*2)) | (0x3<<(6*2)));
			GPFCON |= ((1<<(4*2)) | (1<<(5*2)) | (1<<(6*2)));

			GPFDAT &= ~((1<<4) | (1<<5) | (1<<6));
			break;
		}
		case 1:
		{
			s3c2410_gpio_cfgpin(S3C2410_GPF4,S3C2410_GPF4_OUTP);
			s3c2410_gpio_setpin(S3C2410_GPF4,0);
			break;
		}
		case 2:
		{
			s3c2410_gpio_cfgpin(S3C2410_GPF5,S3C2410_GPF5_OUTP);
			s3c2410_gpio_setpin(S3C2410_GPF5,0);
			break;
		}
		case 3:
		{
			s3c2410_gpio_cfgpin(S3C2410_GPF6,S3C2410_GPF6_OUTP);
			s3c2410_gpio_setpin(S3C2410_GPF6,0);
			break;
		}
	}
	return 0;
}

static ssize_t S3C2440_LEDS_WRITE(struct file *file,const char __user *buf,size_t count,loff_t ppos)
{
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	char val;

	copy_from_user(&val,buf,1);

	switch(minor)
	{
		case 0:
		{
			s3c2410_gpio_setpin(S3C2410_GPF4,(val & 0x1));
			s3c2410_gpio_setpin(S3C2410_GPF5,(val & 0x1));
			s3c2410_gpio_setpin(S3C2410_GPF6,(val & 0x1));
			break;
		}
		case 1:
		{
			s3c2410_gpio_setpin(S3C2410_GPF4,(val));		
			break;
		}
		case 2:
		{
			s3c2410_gpio_setpin(S3C2410_GPF5,(val));		
			break;
		}
		case 3:
		{
			s3c2410_gpio_setpin(S3C2410_GPF6,(val));		
			break;
		}
	}
	return 1;
}

static struct file_operations dev_oper=
{
	.owner	=THIS_MODULE,
	.open	=S3C2440_LEDS_OPEN,
	.write	=S3C2440_LEDS_WRITE,
};

static int __init S3C2440_INIT(void)
{
	int major;
	int i;
	major=register_chrdev(DEV_MAJOR,DEV_NAME,&dev_oper);
	leds_class=class_create(THIS_MODULE,"leds");

	leds_class_device[0]=class_device_create(leds_class, NULL, MADEV(DEV_MAJOR,0), NULL, "leds");             //创建leds并挂载设备驱动号 252 0

	for(i=1;i<4;++i)
	{
		leds_class_device[i]=class_device_create(leds_class, NULL, MADEV(DEV_MAJOR,i) ,NULL ,"led%d", i);     //创建led（1/2/3）并挂载设备驱动号 252 1/2/3
	}

	GPIO_VA=ioremap(0x56000000,0x10000);
	if(!GPIO_VA)
		return -EIO;      //搞懂返回值
}

static int __exit S3C2440_EXIT(void)
{
	int i;
	unregister_chrdev(DEV_MAJOR,DEVNAME);

	for(i=0;i<4;++i)
	{
		class_device_unregister(leds_class_device[i]);
	}

	class_destroy(leds_class);
	iounmap(GPIO_VA);
}

module_init(S3C2440_INIT);		//在使用insmod 调用该函数
module_exit(S3C2440_EXIT);		//在使用rmmod 调用该函数	
MODULE_LICENCE("GPL");