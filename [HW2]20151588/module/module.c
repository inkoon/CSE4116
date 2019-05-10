#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/ioctl.h>
#include <asm/ioctl.h>

#include "./fpga_dot_font.h"

#define IOM_FPGA_DOT_ADDRESS 0x08000210 // pysical address
#define IOM_FND_ADDRESS 0x08000004 // pysical address
#define IOM_LED_ADDRESS 0x08000016 // pysical address
#define IOM_FPGA_TEXT_LCD_ADDRESS 0x08000090 // pysical address - 32 Byte (16 * 2)

#define DEV_MAJOR 242
#define DEV_NAME "dev_driver"

/* Global variables */
static int driver_usage = 0;	// driver usage count
static unsigned char *iom_fpga_fnd_addr;	// fnd device mapping address
static unsigned char *iom_fpga_led_addr;	// led device mapping address
static unsigned char *iom_fpga_dot_addr;	// dot device mapping address
static unsigned char *iom_fpga_text_lcd_addr;	// text lcd device mapping address

/* Define functions */
int dev_open(struct inode *inode, struct file *filp);
ssize_t dev_write(struct file *filp, const char *gdata, size_t length, loff_t *offset);
long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long param);
int dev_release(struct inode *inode, struct file *filp);

/* file operation structure */
struct file_operations driver_fops =
{
	.owner = THIS_MODULE,
	.open = dev_open,
	.write = dev_write,
	.unlocked_ioctl = dev_ioctl,
	.release = dev_release,
};

/* timer structure */
static struct mydata{
	struct timer_list timer;
	int count;
};
struct mydata t;

/* device open function */
int dev_open(struct inode *inode, struct file *filp){
	printk("dev_driver open\n");
	if(driver_usage != 0) return -EBUSY;
	driver_usage = 1;

	return 0;
}

/* write function */
ssize_t dev_write(struct file *filp, const char *gdata, size_t length, loff_t *offset){
	unsigned char value[4];
	unsigned long tmp = (unsigned long)gdata;
	int i;

	printk("dev_driver write\n");
	for(i = 3; 0 <= i; i--){
		value[i] = tmp&0xFF;
		tmp = tmp>>8;
	}
	printk("[%d][%d][%d][%d]\n",value[0],value[1],value[2],value[3]);
	
	// todo

	return 1;
}

/* ioctl function */
long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long param){
	printk("ioctl\n");
	dev_write(filp,(const char *)param,4,0);

	return 1;
}

/* device close function */
int dev_release(struct inode *inode, struct file *filp){
	printk("dev_driver close\n");
	driver_usage = 0;
	return 0;
}

/* insmod function */
int dev_driver_init(void){
	int result;

	printk("driver init\n");
	/* Registering a device */
	result = register_chrdev(DEV_MAJOR,DEV_NAME,&driver_fops);
	if(result < 0){
		printk("driver init error![%d]\n",result);
		return result;
	}
	
	/* Allocate the physical address space to the kernel address space */
	iom_fpga_dot_addr = ioremap(IOM_FPGA_DOT_ADDRESS, 0x10);
	iom_fpga_fnd_addr = ioremap(IOM_FND_ADDRESS, 0x4);
	iom_fpga_led_addr = ioremap(IOM_LED_ADDRESS, 0x1);
	iom_fpga_text_lcd_addr = ioremap(IOM_FPGA_TEXT_LCD_ADDRESS, 0x32);
	/* Init timer */
	init_timer(&(t.timer));

	return 0;
}

/* rmmod function */
void dev_driver_exit(void){
	printk("remove driver\n");

	/* Free the kernel address space */
	iounmap(iom_fpga_text_lcd_addr);
	iounmap(iom_fpga_led_addr);
	iounmap(iom_fpga_fnd_addr);
	iounmap(iom_fpga_dot_addr);
	
	/* Unregistering a device */
	unregister_chrdev(DEV_MAJOR,DEV_NAME);
}

/* Declaration of the init and exit functions */
module_init( dev_driver_init );
module_exit( dev_driver_exit );
