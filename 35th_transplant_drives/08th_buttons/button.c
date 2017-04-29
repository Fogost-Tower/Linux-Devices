/*参考\drivers\input\keyboard\Gpio_keys.c*/


#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <asm/gpio.h>
#include <asm/io.h>
//#include <asm/arch/regs-gpio.h>

struct pin_desc
{
	int irq;
	char *name;
	unsigned int pin;
	unsigned int key_val;
};

struct pin_desc pins_desc[4] = 
{
		{IRQ_EINT0	, "S2" , S3C2410_GPF(0) , KEY_L},
		{IRQ_EINT2	, "S3" , S3C2410_GPF(2) , KEY_S},
		{IRQ_EINT11	, "S4" , S3C2410_GPG(3) , KEY_ENTER},
		{IRQ_EINT19	, "S5" , S3C2410_GPG(11) , KEY_LEFTSHIFT}
};

static struct pin_desc *irq_pd;
static struct input_dev *buttons_dev;
static struct timer_list buttons_timer;

/*中断函数 */
static irqreturn_t buttons_irq(int irq,void *dev_id)
{
	irq_pd=(struct pin_desc *)dev_id;
	mod_timer(&buttons_timer,jiffies+HZ/100);  //delay 10 us
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void buttons_timer_function(unsigned long data)
{
	struct pin_desc *pindesc = irq_pd;
	unsigned int pinval;

	if(NULL==pindesc)
		return;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		//最后一个参数0 表示松开 1反之
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 0);
		input_sync(buttons_dev);
	}
	else 
	{
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 1);
		input_sync(buttons_dev);
	}
	
}

static int buttons_init()
{
	int i;
	/*1.分配一个input_dev struct*/
	buttons_dev = input_allocate_device();

	/*2.setup */
	/*2.1能产生那一类事件*/
	set_bit(EV_KEY,buttons_dev->evbit);
	set_bit(EV_REP,buttons_dev->evbit);
	
	/*2.1能产生那类操作里的那些事件:L,S,ENTER,LEFTSHIFT*/
	set_bit(KEY_L,buttons_dev->keybit);
	set_bit(KEY_S,buttons_dev->keybit);
	set_bit(KEY_ENTER,buttons_dev->keybit);
	set_bit(KEY_LEFTSHIFT,buttons_dev->keybit);

	
	/*3.register*/
	input_register_device(buttons_dev);
	
	/*4.hardware correlation operction*/
	init_timer(&buttons_timer);
	buttons_timer.function = buttons_timer_function;
	add_timer(&buttons_timer);
	
	for(i=0; i<4; i++){
		request_irq(pins_desc[i].irq,buttons_irq,IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,pins_desc[i].name,&pins_desc[i]);
	}
	return 0;
}

static void buttons_exit()
{
	int i;
	for(i=0; i<4; i++)
	{
		free_irq(pins_desc[i].irq ,&pins_desc[i]);
	}
	del_timer(&buttons_timer);
	input_unregister_device(buttons_dev);
	input_free_device(buttons_dev);
	
}

module_init(buttons_init);
modele_exit(buttons_exit);
MODULE_LICENSE("GPL");
