#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <asm/gpio.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/timer.h>

#define IOM_FND_ADDRESS 0x08000004	// physical address
#define MAJOR_NUM 242
#define DEV_NAME "stopwatch"

/* Global variables */
static int major=0, minor=0;
static dev_t dev;
static struct cdev cdev;
static int driver_usage=0;	// driver usage count
static unsigned char *iom_fpga_fnd_addr;	// fnd device mapping address

/* Define functions */
void stopwatch_blink(unsigned long timeout);
void fnd_write(int time);
void stopwatch_end(unsigned long timeout);
static int stopwatch_open(struct inode *, struct file *);
static int stopwatch_release(struct inode *, struct file *);
static int stopwatch_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

/* Wait queue */
wait_queue_head_t wait_q;
DECLARE_WAIT_QUEUE_HEAD(wait_q);

/* interrupt functions */
irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler3(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg);

/* file operation structure */
static struct file_operations stopwatch_fops =
{
	.open = stopwatch_open,
	.write = stopwatch_write,
	.release = stopwatch_release,
};

/* timer structure */
typedef struct time{
	struct timer_list timer;
	int stop;	// 0:start, 1:stop
	int value;	// record time
}time;
struct time t;
struct time end;

irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg) {
	printk(KERN_ALERT "interrupt1!!! = %x\n", gpio_get_value(IMX_GPIO_NR(1, 11)));
	if(t.stop){
		t.timer.expires = get_jiffies_64() + (HZ/100);
		t.timer.data=0;
		t.timer.function = stopwatch_blink;
		t.stop = 0;
		add_timer(&(t.timer));
	}

	return IRQ_HANDLED;
}

irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg) {
	printk(KERN_ALERT "interrupt2!!! = %x\n", gpio_get_value(IMX_GPIO_NR(1, 12)));
    t.stop = 1;
	return IRQ_HANDLED;
}

irqreturn_t inter_handler3(int irq, void* dev_id,struct pt_regs* reg) {
	printk(KERN_ALERT "interrupt3!!! = %x\n", gpio_get_value(IMX_GPIO_NR(2, 15)));
	t.value = 0;
	fnd_write(0);
	t.stop = 1;
	return IRQ_HANDLED;
}

irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg) {
	printk(KERN_ALERT "interrupt4!!! = %x\n", gpio_get_value(IMX_GPIO_NR(5, 14)));
	if(gpio_get_value(IMX_GPIO_NR(5, 14))){
		del_timer(&end.timer);
	}
	else{
		end.timer.expires = get_jiffies_64() + 3*HZ;
		end.timer.data = 0;
		end.timer.function = stopwatch_end;
		add_timer(&(end.timer));
	}
	return IRQ_HANDLED;
}

/* function to be invoked if 't' timer expires */
void stopwatch_blink(unsigned long timeout){
	if(t.stop){
		return;
	}
	t.value++;
	if(t.value == 360000){
		t.value = 0;
	}
	if((t.value%100) == 0){
		fnd_write(t.value/100);
	}
	t.timer.expires = get_jiffies_64()+HZ/100;
	t.timer.data=0;
	t.timer.function = stopwatch_blink;
	add_timer(&(t.timer));
}

/* write the time to the fnd device */
void fnd_write(int time){
	unsigned short int s_value = 0;
	unsigned char fnd[4] = {0};
	int m;
	int s;
	m = time/60; s = time%60;
	fnd[0] = m/10; fnd[1] = m%10;
	fnd[2] = s/10; fnd[3] = s%10;
	s_value = (fnd[0]<<12)|(fnd[1]<<8)|(fnd[2]<<4)|fnd[3];
	outw(s_value,(unsigned int)iom_fpga_fnd_addr);
}

void stopwatch_end(unsigned long timeout){
	fnd_write(0);
	t.value = 0;
	__wake_up(&wait_q,1,1,NULL);
}

static int stopwatch_open(struct inode *minode, struct file *mfile){
	int ret;
	int irq;

	printk(KERN_ALERT "Open Module\n");
	if(driver_usage != 0) return -EBUSY;
	driver_usage = 1;
	t.stop = 1;
	t.value = 0;

	// int1
	gpio_direction_input(IMX_GPIO_NR(1,11));
	irq = gpio_to_irq(IMX_GPIO_NR(1,11));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq,inter_handler1,IRQF_TRIGGER_FALLING,"home",0);

	// int2
	gpio_direction_input(IMX_GPIO_NR(1,12));
	irq = gpio_to_irq(IMX_GPIO_NR(1,12));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq,inter_handler2,IRQF_TRIGGER_FALLING,"back",0);

	// int3
	gpio_direction_input(IMX_GPIO_NR(2,15));
	irq = gpio_to_irq(IMX_GPIO_NR(2,15));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq,inter_handler3,IRQF_TRIGGER_FALLING,"vol+",0);

	// int4
	gpio_direction_input(IMX_GPIO_NR(5,14));
	irq = gpio_to_irq(IMX_GPIO_NR(5,14));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq,inter_handler4,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,"vol-",0);

	return 0;
}

static int stopwatch_release(struct inode *minode, struct file *mfile){
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 11)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 12)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(2, 15)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(5, 14)), NULL);
	//cdev_del(&cdev);
	del_timer(&(t.timer));
	del_timer(&(end.timer));

	driver_usage = 0;
	
	printk(KERN_ALERT "Release Module\n");
	return 0;
}

static int stopwatch_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos ){
	printk("write\n");
	
	interruptible_sleep_on(&wait_q);

	return 0;
}

/* Registering the device */
static int stopwatch_register_cdev(void)
{
	int error;
	int result;
	major = MAJOR_NUM;
	if(major){
		dev = MKDEV(major, minor);
		error = register_chrdev_region(dev,1,"stopwatch");
	}
	else{
		error = alloc_chrdev_region(&dev,minor,1,"stopwatch");
		major = MAJOR(dev);
	}
	if(error<0) {
		printk(KERN_WARNING "stopwatch: can't get major %d\n",major);
		return result;
	}
	printk(KERN_ALERT "major number = %d\n", major);
	cdev_init(&cdev,&stopwatch_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &stopwatch_fops;
	error = cdev_add(&cdev,dev,1);
	if(error)
	{
		printk(KERN_NOTICE "stopwatch Register Error %d\n", error);
	}
	return 0;
}

/* insmod function */
static int __init stopwatch_init(void) {
	int result;
	/*if((result = stopwatch_register_cdev()) < 0 )
		return result;*/
	result = register_chrdev(MAJOR_NUM,DEV_NAME,&stopwatch_fops);
	if(result < 0){
		printk("driver init error![%d]\n",result);
		return result;
	}
	iom_fpga_fnd_addr = ioremap(IOM_FND_ADDRESS,0x4);
	init_timer(&(t.timer));
	init_timer(&(end.timer));
	printk(KERN_ALERT "Init Module Success\n");
	printk(KERN_ALERT "Device : /dev/stopwatch, Major Num : 242\n");
	return 0;
}

/* rmmod function */
static void __exit stopwatch_exit(void) {
	iounmap(iom_fpga_fnd_addr);
	//unregister_chrdev_region(dev, 1);
	driver_usage = 0;
	unregister_chrdev(MAJOR_NUM,DEV_NAME);
	printk(KERN_ALERT "Remove Module Success \n");
}

/*Declaration of the init and exit functions */
module_init(stopwatch_init);
module_exit(stopwatch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KWON");
