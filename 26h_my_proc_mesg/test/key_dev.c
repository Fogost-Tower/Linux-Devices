#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#define ulong unsigned long
#define uchar unsigned char
#define uint unsigned int  

#define DEV_NAME    "key_dev"       //设备名字
#define KEY_MAJOR   251             //设备号

static struct class *key_class;
static struct class_device *key_class_dev;
/***************定义通用口的映射虚拟地址**************************/
static ulong GPIO_VA;
#define GPOFFSET(x) ((x) - 0x56000000)

#define GPFCON  (*(volatile ulong *)(GPIO_VA+GPOFFSET(0x56000050)))
#define GPFDAT  (*(volatile ulong *)(GPIO_VA+GPOFFSET(0x56000054)))
#define GPGCON  (*(volatile ulong *)(GPIO_VA+GPOFFSET(0x56000060)))
#define GPGDAT  (*(volatile ulong *)(GPIO_VA+GPOFFSET(0x56000064)))

static DECLARE_WAIT_QUEUE_HEAD(key_waitq);
static volatile int ev_press=0;

extern int myprintk(const char *fmt, ...);

/*pin descirption struct*/
struct pin_desc
{
	uint pin;
	uint key_val;
};
/*press :0x01,0x02,0x03,0x04  unpress :0x81,0x82,0x83,0x84*/
static uchar key_val;
struct pin_desc pins[4]=
{
	{S3C2410_GPF0,0x01},
	{S3C2410_GPF2,0x02},
	{S3C2410_GPG3,0x03},
	{S3C2410_GPG11,0x04}

};


/*read key button value*/
static irqreturn_t buttons_irq(int irq,void *dev_id)
{
	struct pin_desc *pindesc = (struct pin_desc*)dev_id;
	uint pinval;
	pinval = s3c2410_gpio_getpin(pindesc->pin);
	
	if(pinval)
	{
		key_val=0x80 | pindesc->key_val;
	}
	else
	{
		key_val=pindesc->key_val;
	}

	ev_press = 1;
	wake_up_interruptible(&key_waitq);
	return IRQ_HANDLED;
}

static int S3C2440_KEY_OPEN(struct inode *inode,struct file *file)
{  
	/*search method realize */
	/*config GPF0 GPF2 is out pin*/
	/*config GPG3 GPG11 is out pin*/
	/*GPFCON &= ~((0x3<<(0*2)) | (0x3<<(2*2)));
	GPGCON &= ~((0x3<<(3*2)) | (0x3<<(11*2)));*/

	/*interrput method realize*/	
	request_irq(IRQ_EINT0, buttons_irq,IRQT_BOTHEDGE,"S2",&pins[0]);
	request_irq(IRQ_EINT2, buttons_irq,IRQT_BOTHEDGE,"S3",&pins[1]);
	request_irq(IRQ_EINT11,buttons_irq,IRQT_BOTHEDGE,"S4",&pins[2]);
	request_irq(IRQ_EINT19,buttons_irq,IRQT_BOTHEDGE,"S5",&pins[3]);

	return 0;
}


static int S3C2440_KEY_READ(struct file *filp, char __user *buff,size_t count, loff_t *offp)
{
	/*uchar key_vals[4];
	int regfval;
	int reggval;
	if(count!=sizeof(key_vals))
		return -EINVAL;

	regfval = GPFDAT;
	key_vals[0]= ((regfval) & (1<<0)) ? 1:0;
	key_vals[1]= ((regfval) & (1<<2)) ? 1:0;

	reggval = GPGDAT;
	key_vals[2]= ((reggval) & (1<<3)) ? 1:0;
	key_vals[3]= ((reggval) & (1<<11)) ? 1:0;

	copy_to_user(buff,key_vals,sizeof(key_vals));*/


	if(count!=1)
		return -EINVAL;

	wait_event_interruptible(key_waitq,ev_press);
	
	copy_to_user(buff,&key_val,1);

	ev_press = 0;

	return 1;
}

int S3C2440_KEY_CLOSE(struct inode *inode,struct file *file)
{
	free_irq(IRQ_EINT0,&pins[0]);
	free_irq(IRQ_EINT2,&pins[1]);
	free_irq(IRQ_EINT11,&pins[2]);
	free_irq(IRQ_EINT19,&pins[3]);
	return 0;
}

static struct file_operations key_oper=
{
	.owner  =THIS_MODULE,
	.open   =S3C2440_KEY_OPEN,           //open驱动
//	.write  =S3C2440_KEY_WRITE,          //write驱动
	.read   =S3C2440_KEY_READ,           //read驱动
	.release=S3C2440_KEY_CLOSE
};

static int key_dev_init(void)          //key设备注册
{
	myprintk("asbasdads\n");
		
	GPIO_VA=ioremap(0x56000000,0x10000);
	
	register_chrdev(KEY_MAJOR,DEV_NAME,&key_oper);

	key_class = class_create(THIS_MODULE,"key_dev");

	key_class_dev = class_device_create(key_class,NULL,MKDEV(KEY_MAJOR,0),NULL,"keydev");   //在/dev目录下创建设备节点keydev;

	return 0;
}

static void key_dev_exit(void)         //key设备卸载
{
	unregister_chrdev(KEY_MAJOR,DEV_NAME);
	class_device_unregister(key_class_dev);
	class_destroy(key_class);
	iounmap(GPIO_VA);
}

module_init(key_dev_init);
module_exit(key_dev_exit);
MODULE_LICENSE("GPL");
