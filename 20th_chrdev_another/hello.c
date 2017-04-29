/*
*  reference : 第七个实验
*/

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/major.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/smp_lock.h>
#include <linux/seq_file.h>

#include <linux/kobject.h>
#include <linux/kobj_map.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/backing-dev.h>


#define HELLO_CNT 2

static int major;
static struct cdev hello_cdev;
static struct cdev hello2_cdev;

static struct class *cls;
static struct class_device *cls_dev;


int hello_open(struct inode * inode, struct file * filp)
{
	printk("hello_open \n");
	return 0;
}

int hello2_open(struct inode * inode, struct file * filp)
{
	printk("hello2_open \n");
	return 0;
}


static struct file_operations hello_fops= {
	.owner	= THIS_MODULE,
	.open 	= hello_open,
};

static struct file_operations hello2_fops= {
	.owner	= THIS_MODULE,
	.open 	= hello2_open,
};

static int hello_init(void)
{
	dev_t devid;
#if 0
	major = register_chrdev(0, "hello", &hello_fops);  /*(major ,0 ) , (major ,1 )  ...(major, 255)都对应hello_fops*/
#else

	if(major){
		devid = MKDEV(major,0);
		register_chrdev_region(devid, HELLO_CNT, "hello");   //result (mjor , 0 ) corresponding hello_fops , other(major, 1 ~ 255) not corresponding
	} 
	else{
		alloc_chrdev_region(&devid, 0, HELLO_CNT, "hello");
		major = MAJOR(devid);
	}

	cdev_init(&hello_cdev, &hello_fops);
	cdev_add(&hello_cdev, devid, HELLO_CNT);

	devid = MKDEV(major,2);
	register_chrdev_region(devid, 1, "hello2");
	cdev_init(&hello2_cdev, &hello2_fops);
	cdev_add(&hello2_cdev, devid, 1);

	

#endif

	cls = class_create(THIS_MODULE, "hello");
	class_device_create(cls, NULL, MKDEV(major,0), NULL, "hello_0");
	class_device_create(cls, NULL, MKDEV(major,1), NULL, "hello_1");
	class_device_create(cls, NULL, MKDEV(major,2), NULL, "hello_2");
	class_device_create(cls, NULL, MKDEV(major,3), NULL, "hello_3");
	
	
	return 0;
}

static void hello_exit(void)
{
	class_device_destroy(cls, MKDEV(major, 0));
	class_device_destroy(cls, MKDEV(major, 1));
	class_device_destroy(cls, MKDEV(major, 2));
	class_device_destroy(cls, MKDEV(major, 3));
	class_destroy(cls);
	cdev_del(&hello_cdev);
	unregister_chrdev_region(MKDEV(major , 0), HELLO_CNT);
	cdev_del(&hello2_cdev);
	unregister_chrdev_region(MKDEV(major , 2), 1);
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
