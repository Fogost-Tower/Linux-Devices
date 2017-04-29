/*
*reference s3c2410_ts.c
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>

#define CUR_STATUS 		(adc_p->adcdat0) & (1<<15)

#define BUF_SIZE			(1024 * 1024)
#define INPUT_REPLAY  	0
#define INPUT_TAG  		1

struct adc_regs
{
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
};

static int replay_ioctl (struct inode *, struct file *, unsigned int, unsigned long);
static ssize_t replay_write (struct file *, const char __user *, size_t, loff_t *);
extern int myprintk(const char *fmt, ...);

static struct input_dev *s3c_ts;
static volatile struct adc_regs *adc_p;
static struct timer_list ts_timer;


static int major;
static char *replay_buf;
static int replay_r = 0;
static int replay_w = 0;
static dev_t dev_id;
static struct cdev replay_cdev;
static struct class *replay_class;

static struct timer_list replay_timer;


static struct file_operations replay_fops={
	.owner 	= THIS_MODULE,
	.ioctl	= replay_ioctl,
	.write	= replay_write,
};

void write_input_event_to_flie(unsigned int time, unsigned int type, unsigned int code, int val)
{ 
	myprintk("0x%08x 0x%08x 0x%08x %d\n", time, type, code, val);
}

static void enter_wait_down_mode(void)
{
	adc_p->adctsc = 0xd3;
}

static void enter_wait_up_mode(void)
{
	adc_p->adctsc = 0x1d3;
}

static void enter_measure_xy_mode(void)
{
	adc_p->adctsc |= (1<<3) | (1<<2);
}

static void start_adc(void)
{
	adc_p->adccon |= (1<<0);
}

/*return value : i = 0 fult  ,other value have data*/
static int replay_get_line(char *line)
{
	int cnt = 0;
	
	while(replay_r <= replay_w){
		if ((replay_buf[replay_r] == ' ') || (replay_buf[replay_r] == '\n') || (replay_buf[replay_r] == '\r') || (replay_buf[replay_r] == '\t'))
			replay_r++;
		else
			break;
	}

	while(replay_r <= replay_w){
		if ((replay_buf[replay_r] == '\n') || (replay_buf[replay_r] == '\r')){
			break;		
		}
		else{
			line[cnt]=replay_buf[replay_r];
			++cnt;
			++replay_r;
		}
	}

	line[cnt] = '\0';
	
	return cnt;
	
}


static int replay_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long val)
{
	int buf[100];
	
	switch(cmd){
		case INPUT_REPLAY:
		{
			/*启动回放: 根据replay_buf 里的值*/
			replay_timer.expires = jiffies + 1;
			add_timer(&replay_timer);
			break;
		}
		case INPUT_TAG:
		{
			copy_from_user(buf, (const void __user*)val, 100);
			buf[99] = '\0';
			myprintk("%s \n",buf);
			break;
		}
	}
	
	return 0;
}

static ssize_t replay_write (struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	/*把应用程序传入的数据写入replay_buf*/
	if(replay_w + size >= BUF_SIZE	){
		printk("replay_buf full !\n");
		return -EIO;
	}
	if(copy_from_user(replay_buf + replay_w, buf, size)){
		return -EIO;
	}
	else{
		replay_w += size;
	}
	return size;	
}



static irqreturn_t ts_down_irq(int irq, void *dev_id)
{
	if(CUR_STATUS){
		/*unpress*/
		input_report_abs(s3c_ts, ABS_PRESSURE, 0);
		write_input_event_to_flie(jiffies, EV_ABS, ABS_PRESSURE, 0);
		
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		write_input_event_to_flie(jiffies, EV_KEY, BTN_TOUCH, 0);
		
		input_sync(s3c_ts);
		write_input_event_to_flie(jiffies, EV_SYN, SYN_REPORT, 0);
		
		enter_wait_down_mode();
	}
	else{
		/*press*/
		//enter_wait_up_mode();
		enter_measure_xy_mode();
		start_adc();
	}
	
	return IRQ_HANDLED;
}

static int filter_func(int *x, int *y)
{
#define FILTER_LIMIT 10
	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1]) /2;
	avr_y = (y[0] + y[1]) /2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if((det_x > FILTER_LIMIT) || (det_y > FILTER_LIMIT)){
		return 0;
	}

	avr_x = (x[1] + x[2]) /2;
	avr_y = (y[1] + y[2]) /2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if((det_x > FILTER_LIMIT)|| (det_y > FILTER_LIMIT)){
		return 0;
	}

	return 1;
	
}

static irqreturn_t adc_irq(int irq, void *dev_id)
{
	static int cnt=0;
	static int Xval[4], Yval[4];
	int x_temp, y_temp;

	x_temp = adc_p->adcdat0;
	y_temp = adc_p->adcdat1;
	
	if(CUR_STATUS){
		cnt = 0;
		input_report_abs(s3c_ts, ABS_PRESSURE, 0);
		write_input_event_to_flie(jiffies, EV_ABS, ABS_PRESSURE, 0);
		
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		write_input_event_to_flie(jiffies, EV_KEY, BTN_TOUCH, 0);
		
		input_sync(s3c_ts);
		write_input_event_to_flie(jiffies, EV_SYN, SYN_REPORT, 0);
		
		enter_wait_down_mode();
	}
	else{
		Xval[cnt] = x_temp & 0x3FF;
		Yval[cnt] = y_temp & 0x3FF;
		++cnt;
		
		if(cnt == 4){
			if(filter_func(Xval, Yval)){
				//printk("Xval = %d, Yval = %d\n", (Xval[0] + Xval[1] + Xval[2] + Xval[3]) /4, (Yval[0] + Yval[1] + Yval[2] + Yval[3]) /4);
				input_report_abs(s3c_ts, ABS_X, (Xval[0] + Xval[1] + Xval[2] + Xval[3]) /4);
				write_input_event_to_flie(jiffies, EV_ABS, ABS_X, (Xval[0] + Xval[1] + Xval[2] + Xval[3]) /4);

				input_report_abs(s3c_ts, ABS_Y, (Yval[0] + Yval[1] + Yval[2] + Yval[3]) /4);
				write_input_event_to_flie(jiffies, EV_ABS, ABS_Y, (Yval[0] + Yval[1] + Yval[2] + Yval[3]) /4);

				input_report_abs(s3c_ts, ABS_PRESSURE, 1);
				write_input_event_to_flie(jiffies, EV_ABS, ABS_PRESSURE, 1);

				input_report_key(s3c_ts, BTN_TOUCH, 1);
				write_input_event_to_flie(jiffies, EV_KEY, BTN_TOUCH, 1);
				
				input_sync(s3c_ts);
				write_input_event_to_flie(jiffies, EV_SYN, SYN_REPORT, 0);
			}

			cnt=0;

			enter_wait_up_mode();
			
			mod_timer(&ts_timer, jiffies + HZ/100);
		 }
		else{
			enter_measure_xy_mode();
			start_adc();
		}
	}
	
	return IRQ_HANDLED;
}

static void ts_timer_func(unsigned long data)
{
	if(CUR_STATUS){
		input_report_abs(s3c_ts, ABS_PRESSURE, 0);
		write_input_event_to_flie(jiffies, EV_ABS, ABS_PRESSURE, 0);
		
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		write_input_event_to_flie(jiffies, EV_KEY, BTN_TOUCH, 0);
		
		input_sync(s3c_ts);
		write_input_event_to_flie(jiffies, EV_SYN, SYN_REPORT, 0);
		
		enter_wait_down_mode();
	}
	else{
		enter_measure_xy_mode();
		start_adc();
	}
}

static void replay_timer_func(unsigned long data)
{
	/*把replay_buf 里的一些数据取出来上报*/
	/*读出第一行数据，确定timer值，上报第一行
	*继续都下一个数据，如果它的timer等于第一行的就上报
	*							否则:不上报
	*/

	unsigned int time;
	unsigned int type;
	unsigned int code;
	int val;

	static unsigned int pre_time=0, pre_type=0, pre_code=0;
	static int pre_val = 0;
	
	char line[100];
	int ret;

	if(pre_time !=0){
		input_event(s3c_ts, pre_type, pre_code, pre_val);
	}
	
	while(1){
		ret = replay_get_line(line);
		if(ret == 0){
			printk("end of input replay\n");
			del_timer(&replay_timer);
			pre_time=0, pre_type=0, pre_code=0;
			pre_val = 0;
			replay_r = replay_w;
			break;
		}

		time = 0;
		type = 0;
		code = 0;
		val    = 0;
		sscanf(line, "%x %x %x %d", &time, &type, &code, &val);
		//printk( "%x %x %x %d", time, type, code, val);
		
		if(!time && !type && !code && !val)
			continue;
		else{

			if((pre_time == 0) || (time == pre_time)){
				input_event(s3c_ts, type, code, val);
				if(pre_time == 0)
					pre_time = time;
			}
			else{
				/*根据下一个时间，来修改定时时间*/
				mod_timer(&replay_timer, jiffies+(time - pre_time));

				pre_time = time;
				pre_type = type;
				pre_code = code;
				pre_val   = val;
				
				break;
			}
		}
	}

	
	
}

static int s3c_ts_init(void)
{
	int err=0;
	struct clk *clk;
	
	replay_buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if(!replay_buf){
		printk("can't alloc \n");
		return -EIO;
	}

	/*1.assgin a input_dev struct */
	s3c_ts = input_allocate_device();
	if (!s3c_ts) {
		printk(KERN_ERR "s3c2440: unable to allocate input device\n");
		return -ENOMEM;
	}
	
	/*2.setup struct*/
	set_bit(EV_KEY, s3c_ts->evbit);
	set_bit(EV_ABS, s3c_ts->evbit);

	set_bit(BTN_TOUCH, s3c_ts->keybit);

	input_set_abs_params(s3c_ts, ABS_X, 0 ,0x3FF, 0, 0);
	input_set_abs_params(s3c_ts, ABS_Y, 0 ,0x3FF, 0, 0);
	input_set_abs_params(s3c_ts, ABS_PRESSURE, 0 ,1, 0, 0);

	/*3.register object*/
	input_register_device(s3c_ts);
	
	/*4.hardware correlation of operation*/
	clk = clk_get(NULL, "adc");
	clk_enable(clk);
	
	adc_p = ioremap(0x58000000, sizeof(struct adc_regs));

	/*adccon 
	*bit[1] : READ_START: 0;
	*/
	adc_p->adccon	= (1<<14) | (49<<6);

	adc_p->adcdly = 0xffff;

	err=request_irq(IRQ_TC, ts_down_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	err=request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, "ts_adc", NULL);

	init_timer(&ts_timer);
	ts_timer.function = ts_timer_func;
	add_timer(&ts_timer);

	enter_wait_down_mode();

	if(major){
		printk("register_chrdev_region\n");
		dev_id = MKDEV(major,0);
		register_chrdev_region(dev_id, 1, "input_replay");
	}
	else{
		printk("alloc_chrdev_region\n");
		alloc_chrdev_region(&dev_id, 0, 1, "input_replay");
		major = MAJOR(dev_id);
	}

	cdev_init(&replay_cdev, &replay_fops);
	cdev_add(&replay_cdev, dev_id, 1);

	replay_class = class_create(THIS_MODULE, "input_replay");
	device_create(replay_class, NULL, dev_id, "input_emu");
	
	init_timer(&replay_timer);
	replay_timer.function = replay_timer_func;
	//add_timer(&replay_timer);
	
	return 0;
}

static void s3c_ts_exit(void)
{
	device_destroy( replay_class, dev_id);
	class_destroy(replay_class);
	cdev_del(&replay_cdev);
	unregister_chrdev_region(dev_id, 1);

	free_irq(IRQ_ADC, NULL);
	free_irq(IRQ_TC, NULL);
	iounmap(adc_p);
	input_unregister_device(s3c_ts);
	input_free_device(s3c_ts);
	del_timer(&ts_timer);
	//del_timer(&replay_timer);
	kfree(replay_buf);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");

