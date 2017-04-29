/*
*	driver/hid/usbhid/usbmouse.c
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct input_dev *s3c_dev;
static char *usb_buffer;
static dma_addr_t data_dma;
static int len;
static struct urb *usb_irq;

static struct usb_device_id s3c_usb_mouse_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};

static void usb_mouse_irq_fun(struct urb *urb)
{
	static unsigned char pre_val;
#if 0
	int i;
	static int cnt = 0;

	printk("data cnt = %d \n",cnt++);
	for(i=0 ;i<len; i++)
	{
		printk("%02x",usb_buffer[i]);
		
	}
	printk("\n");
#endif
	/*USB Mouse data implication*/
	/*data[0]: bit0 - lift key       1 press  0 unpress
	*		   bit1 - right key 	  1 press  0 unpress
	*		  bit2 - middle key   1 press  0 unpress
	*/

	if((pre_val & (1<<0)) != (usb_buffer[0] & (1<<0)))
	{
		/*lift key happen change*/
		input_event(s3c_dev, EV_KEY, KEY_L, (usb_buffer[0]  & (1<<0)) ? 1:0);
		input_sync(s3c_dev);
	}
	
	if((pre_val & (1<<1)) != (usb_buffer[0] & (1<<1)))
	{
		/*right key happen change*/
		input_event(s3c_dev, EV_KEY, KEY_S, (usb_buffer[0]  & (1<<1) )? 1:0);
		input_sync(s3c_dev);
	}

	if((pre_val & (1<<2)) != (usb_buffer[0]  & (1<<2)))
	{
		/*middle key happen change*/
		input_event(s3c_dev, EV_KEY, KEY_ENTER, (usb_buffer[0]  & (1<<2)) ? 1:0);
		input_sync(s3c_dev);
	}
	pre_val = usb_buffer[0];
	/*reset submit urb*/
	usb_submit_urb(usb_irq,GFP_ATOMIC);
}

static int s3c_usb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{	
	int pipe;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	/*
	printk("found usb mouse\n");
	printk(" bcdUSB = %x \n",dev->descriptor.bcdUSB);
	printk(" pid = %x \n",dev->descriptor.idProduct);
	printk(" vid = %x \n",dev->descriptor.idVendor);
	*/
	interface = intf->cur_altsetting;
	endpoint = &interface->endpoint[0].desc;

	/*1.assign input_dev struct*/
	s3c_dev = input_allocate_device();

	/*2.setup*/
	/*2.1can product who class event*/
	set_bit(EV_KEY,s3c_dev->evbit);
	set_bit(EV_REP,s3c_dev->evbit);   //repeat class event
	/*2.2can product who some event*/
	set_bit(KEY_L,s3c_dev->keybit);
	set_bit(KEY_S,s3c_dev->keybit);
	set_bit(KEY_ENTER,s3c_dev->keybit);

	/*3.register*/
	input_register_device(s3c_dev);
	
	/*4.headware correlative opeaction*/
	/*4.1data transmit third factor
	*	source : USB device certain endpoint
	*	destnation : USB device 
	*	length
	**/
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);  //source
	len = endpoint->wMaxPacketSize;  //length
	usb_buffer = usb_buffer_alloc(dev, len, GFP_ATOMIC, &data_dma); //destnation
	
	/*use third factor*/
	/*assign usb request urb*/
	usb_irq = usb_alloc_urb(0, GFP_KERNEL);
	/*use three factor set urb*/
	usb_fill_int_urb(usb_irq, dev, pipe, usb_buffer,len,
			 usb_mouse_irq_fun, NULL, endpoint->bInterval);
	usb_irq->transfer_dma = data_dma;
	usb_irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/*used urb*/
	usb_submit_urb (usb_irq, GFP_ATOMIC);
	
	return 0;
	
}

static void s3c_usb_mouse_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	//printk("disconnect usbmouse\n");
	usb_kill_urb(usb_irq);
	usb_free_urb(usb_irq);
	
	usb_buffer_free(dev, len, usb_buffer, data_dma);
	input_unregister_device(s3c_dev);
	input_free_device(s3c_dev);
}


static struct usb_driver s3c_usb_mouse_driver = {
	.name		= "s3c_usb_mouse",
	.probe		= s3c_usb_mouse_probe,
	.disconnect	= s3c_usb_mouse_disconnect,
	.id_table	= s3c_usb_mouse_id_table,
};

static int s3c_usb_init(void)
{
	usb_register(&s3c_usb_mouse_driver);
	return 0;
}

static void s3c_usb_exit(void)
{
	usb_deregister(&s3c_usb_mouse_driver);
}

module_init(s3c_usb_init);
module_exit(s3c_usb_exit);
MODULE_LICENSE("GPL");
