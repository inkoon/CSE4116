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
#define ID_LEN 8
#define NAME_LEN 11

/* Global variables */
static int driver_usage = 0;	// driver usage count
static unsigned char *iom_fpga_fnd_addr;	// fnd device mapping address
static unsigned char *iom_fpga_led_addr;	// led device mapping address
static unsigned char *iom_fpga_dot_addr;	// dot device mapping address
static unsigned char *iom_fpga_text_lcd_addr;	// text lcd device mapping address
unsigned char LCD1[] = "20151588";	// my student number(for text lcd)
unsigned char LCD2[] = "In Kwon Lee";	// my name(for text lcd)

/* Define functions */
int dev_open(struct inode *inode, struct file *filp);
ssize_t dev_write(struct file *filp, const char *gdata, size_t length, loff_t *offset);
void kernel_timer_blink(unsigned long timeout);
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
typedef struct mydata{
	struct timer_list timer;
	int count;
}mydata;
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
	unsigned long data;
	int i;

	printk("dev_driver write\n");
	for(i = 3; 0 <= i; i--){
		value[i] = tmp&0xFF;
		tmp = tmp>>8;
	}
	
	/* timer data set */
	data = value[3]<<24|value[0]<<16|value[1]<<8|value[2];

	/* timer set */
	t.count = value[3];
	t.timer.expires = get_jiffies_64() + (HZ/10)*(int)value[2];
	t.timer.data = data;
	t.timer.function = kernel_timer_blink;

	add_timer(&(t.timer));

	return 1;
}

/* function to be invoked if timer expires */
void kernel_timer_blink(unsigned long timeout){
	unsigned char start;
	unsigned char value;
	unsigned char hz;
	unsigned char start_c;
	unsigned char term_c;
	int i;
	unsigned long tmp = t.timer.data;
	unsigned char fnd[4] = {0};
	unsigned char led;
	unsigned char lcd[33];
	int text1_i;
	int text2_i;
	unsigned short int s_val = 0;
	if((t.count--) <= 0) return;
	printk("timer blink[%d]\n",t.count+1);

	/* timer set */
	hz = tmp&0xFF;
	tmp = tmp>>8;
	value = tmp&0xFF;
	tmp = tmp>>8;
	start = tmp&0xFF;
	tmp = tmp>>8;
	start_c = tmp&0xFF;
	t.timer.expires = get_jiffies_64() + (HZ/10)*hz;

	/* fnd */
	fnd[start] = value;
	s_val = fnd[0]<<12|fnd[1]<<8|fnd[2]<<4|fnd[3];
	outw(s_val,(unsigned int)iom_fpga_fnd_addr);
	/* led */
	led = 1<<(8-value);
	s_val = (unsigned short)led;
	outw(s_val,(unsigned int)iom_fpga_led_addr);
	/* dot */
	for(i=0; i<10; i++){
		s_val = fpga_number[value][i]&0x7F;
		outw(s_val,(unsigned int)iom_fpga_dot_addr+i*2);
	}
	/* text lcd */
	term_c = start_c - t.count - 1;
	if((term_c/(16-ID_LEN))%2) text1_i = (16-ID_LEN) - (term_c%(16-ID_LEN));
	else text1_i = term_c%(16-ID_LEN);
	if((term_c/(16-NAME_LEN))%2) text2_i = (16-NAME_LEN) - (term_c%(16-NAME_LEN));
	else text2_i = term_c%(16-NAME_LEN);
	
	for(i=0;i<32;i++) lcd[i] = ' ';
	for(i=0;i<ID_LEN;i++) lcd[i+text1_i] = LCD1[i];
	for(i=0;i<NAME_LEN;i++) lcd[i+text2_i+16] = LCD2[i];
	printk("SSS:%s\n",lcd);
	for(i=0;i<32;i++){
		s_val = (lcd[i]&0xFF)<<8|(lcd[i+1]&0xFF);
		outw(s_val,(unsigned int)iom_fpga_text_lcd_addr+i);
		i++;
	}

	/* get next value */
	value = value%8+1;
	if((term_c+1)%8 == 0){
		start = (start+1)%4;
	}
	tmp = start_c<<24|start<<16|value<<8|hz;
	t.timer.data = tmp;

	add_timer(&(t.timer));
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

	printk("install driver, major number %d\n",DEV_MAJOR);
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
	driver_usage = 0;

	/* Free the kernel address space */
	iounmap(iom_fpga_text_lcd_addr);
	iounmap(iom_fpga_led_addr);
	iounmap(iom_fpga_fnd_addr);
	iounmap(iom_fpga_dot_addr);
	/* Delete timer */
	del_timer_sync(&(t.timer));

	/* Unregistering a device */
	unregister_chrdev(DEV_MAJOR,DEV_NAME);
}

/* Declaration of the init and exit functions */
module_init( dev_driver_init );
module_exit( dev_driver_exit );

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KWON");
