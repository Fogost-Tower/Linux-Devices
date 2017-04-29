/*
*reference driver\block\xd.c  AND driver\block\z2ram.c
*
*/
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/dma.h>

#define RAMBLCOK_SIZE (1024 * 1024)

static struct gendisk *ramblock_disk;
static request_queue_t *ramblock_queue;
static DEFINE_SPINLOCK(ramblock_lock);
static int major;
static unsigned char *ramblock_buf;

static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/*capacity = heads * cylinders * sector *512*/
	geo->heads 		= 2;
	geo->sectors 	= 32;
	geo->cylinders 	= RAMBLCOK_SIZE /2/32/512;
	return 0;
}

static struct block_device_operations ramblock_fops = {
	.owner	= THIS_MODULE,
	.getgeo   = ramblock_getgeo,
};

static void do_ramblock_request (request_queue_t * q)
{
	static int r_cnt = 0;
	static int w_cnt = 0;
	struct request *req;
	//printk(" this do_ramblock_requsest() \n");
 
	while ((req = elv_next_request(q)) != NULL) 
	{
		/*data transmit factor*/
		/* 	source : 
		*  	destination:
		*	length: 
		*/
		unsigned long offset 	= req->sector * 512;                   //source
		//reg->buffer
		unsigned long len 		= req->current_nr_sectors * 512;  //length

		if(rq_data_dir(req) == READ)
		{
			printk(" this do_ramblock_requsest() read %d \n",r_cnt++);
			memcpy(req->buffer, ramblock_buf+offset, len);
		}
		else 
		{
			printk(" this do_ramblock_requsest() write %d \n",w_cnt++);
			memcpy(ramblock_buf+offset, req->buffer, len);
		}
		
		end_request(req, 1);	
	}
}


static int ramblock_init(void)
{
	/* 1.assign a gendisk struct */
	ramblock_disk = alloc_disk(16);  /*minor number  : partition number +1 */
	
	/*2.setup*/
	/*2.1 assign  / setup queue : supply read/write ability*/
	ramblock_queue = blk_init_queue(do_ramblock_request,&ramblock_buf); 

	ramblock_disk->queue = ramblock_queue;	
	/*2.2 setup other property : example capacity*/
	major = register_blkdev(0,"ramblock");  /*cat /proc/devices*/
	
	ramblock_disk->major 		= XT_DISK_MAJOR;
	ramblock_disk->first_minor 	= 0;
	sprintf(ramblock_disk->disk_name,"ramblock");
	ramblock_disk->fops 			= &ramblock_fops;
	set_capacity(ramblock_disk,RAMBLCOK_SIZE /512);

	/*3.headware correlative operation */
	ramblock_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	
	/*4.register*/
	add_disk(ramblock_disk);
	return 0;	
}	

static void ramblock_exit(void)
{
	unregister_blkdev(major,"ramblock");
	del_gendisk(ramblock_disk);
	put_disk(ramblock_disk);
	blk_cleanup_queue(ramblock_queue);
	kfree(ramblock_buf);	
}

module_init(ramblock_init);
module_exit(ramblock_exit);
MODULE_LICENSE("GPL");
