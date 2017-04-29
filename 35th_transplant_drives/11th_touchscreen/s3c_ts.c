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

//#include <asm/plat-s3c24xx/ts.h>

//#include <asm/arch/regs-adc.h>
//#include <asm/arch/regs-gpio.h>

#define CUR_STATUS 		(adc_p->adcdat0) & (1<<15)

struct adc_regs
{
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
};

static struct input_dev *s3c_ts;
static volatile struct adc_regs *adc_p;
static struct timer_list ts_timer;

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

static irqreturn_t ts_down_irq(int irq, void *dev_id)
{
	if(CUR_STATUS){
		/*unpress*/
		input_report_abs(s3c_ts, ABS_PRESSURE, 0);
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		input_sync(s3c_ts);
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
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		input_sync(s3c_ts);
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
				input_report_abs(s3c_ts, ABS_Y, (Yval[0] + Yval[1] + Yval[2] + Yval[3]) /4);
				input_report_abs(s3c_ts, ABS_PRESSURE, 1);
				input_report_key(s3c_ts, BTN_TOUCH, 1);
				input_sync(s3c_ts);
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

static void ts_timer_func(void)
{
	if(CUR_STATUS){
		input_report_abs(s3c_ts, ABS_PRESSURE, 0);
		input_report_key(s3c_ts, BTN_TOUCH, 0);
		input_sync(s3c_ts);
		enter_wait_down_mode();
	}
	else{
		enter_measure_xy_mode();
		start_adc();
	}
}

static int s3c_ts_init(void)
{
	struct clk *clk;
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

	request_irq(IRQ_TC, ts_down_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, "ts_adc", NULL);

	init_timer(&ts_timer);
	ts_timer.function = ts_timer_func;
	add_timer(&ts_timer);

	enter_wait_down_mode();
	
	return 0;
}

static void s3c_ts_exit(void)
{
	free_irq(IRQ_ADC, NULL);
	free_irq(IRQ_TC, NULL);
	iounmap(adc_p);
	input_unregister_device(s3c_ts);
	input_free_device(s3c_ts);
	del_timer(&ts_timer);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");

