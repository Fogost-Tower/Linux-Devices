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
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>

#define MEM_CPY_NO_DMA 	0
#define MEM_CPY_DMA 		1
#define BUF_SIZE				(512 * 1024)
#define DMA0_BASE_ADDR 	0x4B000000
#define DMA1_BASE_ADDR 	0x4B000040
#define DMA2_BASE_ADDR 	0x4B000080
#define DMA3_BASE_ADDR 	0x4B0000C0


static char *src;
static u32 src_phys;

static char *dst;
static u32 dst_phys;

static int major;
static dev_t  devid;
static struct cdev dma_cdev;
static struct class *dma_class;

struct dma_regs
{
	unsigned long disrc;
	unsigned long disrcc;
	unsigned long didst ;
	unsigned long didstc;
	unsigned long dcon;
	unsigned long dstat;
	unsigned long dcsrc;
	unsigned long dcdst;
	unsigned long dmasktrig;
};

static volatile struct dma_regs *dma_regs_p;

static DECLARE_WAIT_QUEUE_HEAD(dma_waitq);
static volatile int ev_dma=0;

static irqreturn_t s3c_dma_irq(int irq,void *dev_id)
{
	/*wake up*/
	ev_dma = 1;
	wake_up_interruptible(&dma_waitq);
	return IRQ_HANDLED;
}

static int s3c_dma_ioctl(struct inode * inode, struct file * filep,
		unsigned int cmd, unsigned long arg)
{
	int i;

	memset(src, 0xAA, BUF_SIZE);
	memset(dst, 0x55, BUF_SIZE);
	
	switch(cmd){
		case MEM_CPY_NO_DMA:
		{
			for(i=0; i<BUF_SIZE; i++)
				dst[i] = src[i];
			if(memcmp(src, dst, BUF_SIZE) == 0){
				printk("MEM_CPY_NO_DMA OK \n");
			}
			else{
				printk("MEM_CPY_NO_DMA ERROR \n");
			}
			break;
		}
		
		case MEM_CPY_DMA:
		{
			ev_dma = 0;
			/*setup source destination size for dma*/
			dma_regs_p->disrc= src_phys;  /*source physical address*/
			dma_regs_p->disrcc	= (0<<1) | (0<<0); /*in the system bus ,increment */
			dma_regs_p->didst 	= dst_phys; /*destination physical address */
			dma_regs_p->didstc	= (0<<2) |(0<<1) | (0<<0);
			dma_regs_p->dcon	= (1<<30) |(1<<29) |(0<<28) | (1<<27) | (0<<23) | (0<<20) | (BUF_SIZE<<0); /*enable interrupt*/

			/*power up*/
			dma_regs_p->dmasktrig = (1<<1) | (1<<0);  

			/**/

			/*sleep*/
			wait_event_interruptible(dma_waitq, ev_dma);

			if(memcmp(src, dst, BUF_SIZE) == 0){
				printk("MEM_CPY_DMA OK \n");
			}
			else{
				printk("MEM_CPY_DMA ERROR \n");
			}
			
			break;
		}
	}

	
	return 0;
}


static struct file_operations s3c_dma_fops={
	.owner 	= THIS_MODULE,
	.ioctl	= s3c_dma_ioctl,
};

static int s3c_dma_init(void)
{

	if(request_irq(IRQ_DMA3, s3c_dma_irq, 0, "s3c_dma", 1)){
		printk("can't request_irq for DAM \n");
		return -EBUSY;
	}
	
		
	/*alloc SRC,DST correlation of buffer remain*/
	src = dma_alloc_writecombine(NULL, BUF_SIZE, &src_phys, GFP_KERNEL);
	if(NULL == src){
		free_irq(IRQ_DMA3, 1);
		dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
		printk("can't alloc buffer for src \n");
		return -ENOMEM;
	}
	dst = dma_alloc_writecombine(NULL, BUF_SIZE, &dst_phys, GFP_KERNEL);
	if(NULL == dst){
		free_irq(IRQ_DMA3, 1);
		dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
		printk("can't alloc buffer for dst \n");
		return -ENOMEM;
	}

	if(major){
		devid = MKDEV(major,0);
		register_chrdev_region(devid, 1, "dma");   //result (mjor , 0 ) corresponding hello_fops , other(major, 1 ~ 255) not corresponding
	} 
	else{
		alloc_chrdev_region(&devid, 0, 1, "dma");
		major = MAJOR(devid);
	}

	cdev_init(&dma_cdev, &s3c_dma_fops);
	cdev_add(&dma_cdev, devid, 1);

	dma_class = class_create(THIS_MODULE, "dma");
	class_device_create(dma_class, NULL, devid, NULL, "dma_0");  // /dev/dma_0

	dma_regs_p = ioremap(DMA3_BASE_ADDR, sizeof(struct dma_regs));
	
	return 0;	

}

static void s3c_dma_exit(void)
{
	class_device_destroy(dma_class, devid);
	class_destroy(dma_class);
	cdev_del(&dma_cdev);
	unregister_chrdev_region(devid, 1);

	dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
	dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
	free_irq(IRQ_DMA3, 1);
}

module_init(s3c_dma_init);
module_exit(s3c_dma_exit);
MODULE_LICENSE("GPL");
