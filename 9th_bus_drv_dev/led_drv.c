/*assign / setup / register a paltform_driver struct */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>

static int major;
static struct class *led_class;
static struct class_device *led_class_dev;
static volatile unsigned long *gpio_con;
static volatile unsigned long *gpio_dat;
static int pin;

static int led_open(struct inode *inode, struct file *file)
{
	/*configuration gpio-f is out pin */
	*gpio_con &= ~(0x2 << (pin *2));
	*gpio_dat |= (0x1 <<(pin));
	return 0;
}

static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t ppos)
{
	char val;

	copy_from_user(&val,buf,1);

	if(val ==1)
	{
		/*¿ªµÆ*/
		*gpio_dat &= ~(1<<pin);
	}
	else
	{
		/*ÃðµÆ*/
		*gpio_dat |= (1<<pin);
	}
	
	return 1;
}

static struct file_operations led_fops =
{
	 .owner	= THIS_MODULE,
	 .open	= led_open,
	 .write 	= led_write,
};

static int led_probe(struct platform_device *pdev)
{
	struct resource *io;
	/*according platform_device de resource process ioremap */
	io = platform_get_resource(pdev,IORESOURCE_MEM,0);
	
	gpio_con = ioremap(io->start, io->end -io->start +1);
	gpio_dat = gpio_con+1;

	io = platform_get_resource(pdev,IORESOURCE_IRQ,0);
	pin = io->start;
	/*register char device driver routine*/

	printk("led_probe,found led \n");

	major = register_chrdev(0, "myled", &led_fops);

	led_class = class_create(THIS_MODULE,"led_dev");

	led_class_dev = class_device_create(led_class,NULL,MKDEV(major,0),NULL,"leddev");

	return 0;
}

static int led_remove()
{
	/*uninstall char device drvier routine*/
	class_device_unregister(led_class_dev);
	class_destroy(led_class);
	unregister_chrdev(major,"myled");
	/*iounmap*/
	iounmap(gpio_con);

	printk("led_remove,found remove\n");
	return 0;
}


struct platform_driver led_drv=
{
	.probe 	= led_probe,
	.remove	= led_remove,
	.driver	={
		.name	= "s3c2440-leds"
	}
};

static int led_drv_init(void)
{
	platform_driver_register(&led_drv);
	return 0;
}

static void led_drv_exit(void)
{
	platform_driver_unregister(&led_drv);
}

module_init(led_drv_init);
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");