/************************************************************************
*Versions:		1.0
*Create Time:	2016.9.26
*writer:		Mr.luo
*Function_Description:
*************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <asm/io.h>

#define ulong unsigned long 
#define uchar unsigned char
#define uint  unsigned int

#define DEV_NAME	"Key_dev"
#define DEV_MAJOR	251

#define GPOFFSET(x) ((x) - 0x56000000)
#define GPFCON		(*(volatile ulong *)GPVA+GPOFFSET(0x56000050);
#define GPFDAT		(*(volatile ulong *)GPVA+GPOFFSET(0x56000054);
#define GPGCON		(*(volatile ulong *)GPVA+GPOFFSET(0x56000060);
#define GPGDAT		(*(volatile ulong *)GPVA+GPOFFSET(0x56000064);

static uint key_val; 
static DECLARE_WAIT_QUEUE_HEAD(Key_waitq);
static volatile int ev_press = 0;
static struct class *key_class;
static struct class_device *key_class_dev;
static struct fasync_struct *key_async_queue;
static atomic_t canopen = ATOMIC_INIT(1);
static ulong GPVA;

typedef struct
{
	uint pin;
	uint key_val;
}pin_desc;

pin_desc Pin_desc[4]=
{
	{S3C2410_GPF0,0x01},
	{S3C2410_GPF2,0x02},
	{S3C2410_GPG3,0x03},
	{S3C2410_GPG11,0x04}
};

static irqreturn_t S3C2440_Buttons_Irq(int irq,void *dev_id)
{
	pin_desc *pindesc = (pin_desc *)dev_id;
	uint pinval;
	
	pinval=s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		key_val = pindesc->key_val;
	}
	
	ev_press = 1;
	wake_up_interruptible(&Key_waitq);
	
	kill_fasync(&key_async_queue,SIGIO,POLL_IN);    //POLL_IN expression have data need read

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int S3C2440_OPEN(struct inode *inode,struct file *file)
{
	if(!atomic_dec_and_test(&canopen))
	{
		atomic_inc(&canopen);
		return -EBUSY;
	}
	request_irq(IRQ_EINT0,S3C2440_Buttons_Irq,IRQT_BOTHEDGE,"S2",&Pin_desc[0]);
	request_irq(IRQ_EINT2,S3C2440_Buttons_Irq,IRQT_BOTHEDGE,"S3",&Pin_desc[1]);
	request_irq(IRQ_EINT11,S3C2440_Buttons_Irq,IRQT_BOTHEDGE,"S4",&Pin_desc[2]);
	request_irq(IRQ_EINT19,S3C2440_Buttons_Irq,IRQT_BOTHEDGE,"S5",&Pin_desc[3]);
	return 0;
}

ssize_t S3C2440_READ(struct file *file,char __user *buf,size_t size,loff_t *ppos)
{
	if(size != 1)
		return -EINVAL;

	wait_event_interruptible(Key_waitq,ev_press);
	copy_to_user(buf,&key_val,1);
	ev_press=0;
	return 1;
}

int S3C2440_RELEASE(struct inode *inode,struct file *file)
{
	atomic_inc(&canopen);
	free_irq(IRQ_EINT0,&Pin_desc[0]);
	free_irq(IRQ_EINT2,&Pin_desc[1]);
	free_irq(IRQ_EINT11,&Pin_desc[2]);
	free_irq(IRQ_EINT19,&Pin_desc[3]); 
}

static uint S3C2440_POLL(struct file *file,poll_table *wait)
{
	uint mask=0;
	poll_wait(file,&Key_waitq,wait);

	if(ev_press)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static int S3C2440_FASYNC(int fd,struct file *filp,int on)
{
	printk("key_dev_test \n");
	return fasync_helper(fd,filp,on,&key_async_queue);
}

static struct file_operations key_drv_fops=
{
	.owner	 =THIS_MODULE,               //这是一个宏，推向编译模块时自动创建__this_module变量
	.open	 =S3C2440_OPEN,
	.read	 =S3C2440_READ,
	.release =S3C2440_RELEASE,
	.poll	 =S3C2440_POLL,
	.fasync  =S3C2440_FASYNC
};


static int S3C2440_INIT(void)
{
	GPVA=ioremap(0x56000000,0x10000);
	
	register_chrdev(DEV_MAJOR,DEV_NAME,&key_drv_fops);

	key_class = class_create(THIS_MODULE,"key_dev");

	key_class_dev = class_device_create(key_class,NULL,MKDEV(DEV_MAJOR,0),NULL,"keydev");   //在/dev目录下创建设备节点keydev;
	
	return 0;
}

static void S3C2440_EXIT(void)
{
	unregister_chrdev(DEV_MAJOR,DEV_NAME);
	
	class_device_unregister(key_class_dev);
	
	class_destroy(key_class);
	
	iounmap(GPVA);
}

module_init(S3C2440_INIT);
module_exit(S3C2440_EXIT);
MODULE_LICENSE("GPL");
