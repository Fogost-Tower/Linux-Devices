/*
*reference \fs\proc_misc.c
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>

#define MYLOG_SIZE	1024

static struct proc_dir_entry *myentry;
static char mylog_buf[MYLOG_SIZE];
static char temp_buf[MYLOG_SIZE];
static int mylog_w 	= 0;
static int mylog_r		= 0;
static int mylog_r_for_read = 0;

static DECLARE_WAIT_QUEUE_HEAD(my_waitq);

static int is_mylog_empty(void)
{
	return (mylog_w == mylog_r);
}

static int is_mylog_empty_for_read(void)
{
	return (mylog_w == mylog_r_for_read);
}

static int is_mylog_full(void)
{
	return ( (mylog_w+1) % MYLOG_SIZE == mylog_r );
}

static int mylog_putc(char c)
{
	if(is_mylog_full()){
		mylog_r = (mylog_r + 1) % MYLOG_SIZE;  //dircard a data bit

		if((mylog_r_for_read + 1) % MYLOG_SIZE == mylog_r){
			mylog_r_for_read = mylog_r;
		}
	}

	mylog_buf[mylog_w] = c;
	mylog_w = (mylog_w + 1) % MYLOG_SIZE;

	wake_up_interruptible(&my_waitq);
}

static int mylog_getc(char *p)
{
	if(is_mylog_empty())
		return 0;
	*p = mylog_buf[mylog_r];
	mylog_r = (mylog_r + 1) % MYLOG_SIZE;
	return 1;
}

static int mylog_getc_for_read(char *p)
{
	if(is_mylog_empty_for_read())
		return 0;
	*p = mylog_buf[mylog_r_for_read];
	mylog_r_for_read= (mylog_r_for_read+ 1) % MYLOG_SIZE;
	return 1;
}

int myprintk(const char *fmt, ...)
{
	va_list args;
	int i;
	int j;
	
	va_start(args, fmt);
	i=vsnprintf(temp_buf, INT_MAX, fmt, args);
	va_end(args);

	for(j=0; j<i; j++){
		mylog_putc(temp_buf[j]);
	}
	
	return i;
}

static ssize_t mymsg_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	int i=0;
	int error = 0;
	char c;
	/*mylog_buf of data copy_to_user ,return */
	if ((file->f_flags & O_NONBLOCK) && !is_mylog_empty_for_read())
		return -EAGAIN;

	error = wait_event_interruptible(my_waitq, !is_mylog_empty_for_read());

	/*copy_to_user*/
	while(!error && (mylog_getc_for_read(&c)) && i<count){
		error = __put_user(c, buf);
		buf++;
		i++;
	}

	if(!error)
		error = i;
	
	return error;
}

static int mymsg_open(struct inode *inode, struct file *file)
{
	mylog_r_for_read = mylog_r;
	return 0;
}
			 
const struct file_operations my_proc_fops={
	.owner 	= THIS_MODULE,
	.read	= mymsg_read,
	.open 	= mymsg_open,
};

static int s3c_proc_init(void)
{

	myentry = create_proc_entry("mymsg", S_IRUSR, &proc_root);
	if (myentry)
		myentry->proc_fops = &my_proc_fops; 
	return 0;
}

static void s3c_proc_exit(void)
{
	remove_proc_entry("mymsg", &proc_root);
}


module_init(s3c_proc_init);
module_exit(s3c_proc_exit);
EXPORT_SYMBOL(myprintk);
MODULE_LICENSE("GPL");
