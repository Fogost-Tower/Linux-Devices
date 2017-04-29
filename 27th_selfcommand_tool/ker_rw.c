#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/poll.h>
#include <linux/device.h>

#define MINOR_CNT 		1
#define KER_RW_R8 		0
#define KER_RW_R16 	1
#define KER_RW_R32		2

#define KER_RW_W8		3
#define KER_RW_W16 	4
#define KER_RW_W32 	5 

static dev_t devid;
static int major;
static struct cdev ker_cdev;
static struct class *ker_class;
static struct class_device *ker_class_dev;


static int ker_rw_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	volatile unsigned char *p8;
	volatile unsigned short *p16;
	volatile unsigned int *p32;

	unsigned int val;
	unsigned int addr;
	unsigned int buf[2];

	copy_from_user(buf, (const void __user *)arg, 8);
	addr	= buf[0];
	val	= buf[1]; 
	
	p8 = (volatile unsigned char *)ioremap(addr, 4);
	p16 = p8;
	p32 = p16;

	switch(cmd){
		case KER_RW_R8:
		{
			val = *p8;
			copy_to_user((void __user *)(arg+4), &val, 4);
			break;
		}
		case KER_RW_R16:
		{
			val = *p16;
			copy_to_user((void __user *)(arg+4), &val, 4);
			break;
		}

		case KER_RW_R32:
		{
			val = *p32;
			copy_to_user((void __user *)(arg+4), &val, 4);
			break;
		}

		case KER_RW_W8:
		{
			*p8 = val;
			break;
		}

		case KER_RW_W16:
		{
			*p16 = val;
			break;
		}

		case KER_RW_W32:
		{
			*p32 = val;
			break;
		}
	}

	iounmap(p8);

	return 0;
}


static struct file_operations ker_rw_fops={
	.owner 	= THIS_MODULE,
	.ioctl	= ker_rw_ioctl,
};

static int ker_rw_init(void)
{

	if(major){
		devid = MKDEV(major,0);
		register_chrdev_region(devid, MINOR_CNT, "ker_rw");   //result (mjor , 0 ) corresponding hello_fops , other(major, 1 ~ 255) not corresponding
	} 
	else{
		alloc_chrdev_region(&devid, 0, MINOR_CNT, "ker_rw");
		major = MAJOR(devid);
	}

	cdev_init(&ker_cdev, &ker_rw_fops);
	cdev_add(&ker_cdev, devid, MINOR_CNT);


	ker_class = class_create(THIS_MODULE, "ker_rw");	
	ker_class_dev= class_device_create(ker_class, NULL, devid, NULL, "ker_rw");
	return 0;

}

static void ker_rw_exit(void)
{
	class_device_unregister(ker_class_dev);
	class_destroy(ker_class);
	cdev_del(&ker_cdev);
	unregister_chrdev_region(devid, MINOR_CNT);
}

module_init(ker_rw_init);
module_exit(ker_rw_exit);
MODULE_LICENSE("GPL");

