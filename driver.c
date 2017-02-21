#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/ioctl.h>


#define MYDEV_NAME "a5"
#define ramdisk_size (size_t) (16 * PAGE_SIZE)

#define CDRV_IOC_MAGIC 'Z'
#define E2_IOCMODE1 _IOWR(CDRV_IOC_MAGIC, 1, int)
#define E2_IOCMODE2 _IOWR(CDRV_IOC_MAGIC, 2, int)

#define MODE1 1 // define mode here. There can be 2 modes
#define MODE2 2

static int majorNo = 700 , minorNo = 0;

static struct class *cl;
struct e2_dev {
	struct cdev cdev;
	char *ramdisk;
	struct semaphore sem1, sem2;
	int count1, count2;
	int mode;
    wait_queue_head_t queue1, queue2;
};

struct e2_dev *dev;


int e2_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "Device opened\n");    
	struct e2_dev *devc = container_of(inode->i_cdev, struct e2_dev, cdev);
    filp->private_data = devc;
    down_interruptible(&devc->sem1);
    if (devc->mode == MODE1) {
        devc->count1++;
		printk(KERN_ALERT "Count 1 is:%d\n", devc->count1); 
        up(&devc->sem1);
        down_interruptible(&devc->sem2);
        return 0;
    }
    else if (devc->mode == MODE2) {
        devc->count2++;
		printk(KERN_ALERT "Count 2 is:%d\n", devc->count2);
    }
    up(&devc->sem1);
	printk(KERN_ALERT "Mode: %d\n",dev->mode);
    return 0;
}

int e2_release(struct inode *inode, struct file *filp)
{
    struct e2_dev *devc = container_of(inode->i_cdev, struct e2_dev, cdev);
    down_interruptible(&devc->sem1);
    if (devc->mode == MODE1) {
        devc->count1--;
        if (devc->count1 == 1)
            wake_up_interruptible(&(devc->queue1));
			up(&devc->sem2);
    }
    else if (devc->mode == MODE2) {
        devc->count2--;
        if (devc->count2 == 1)
            wake_up_interruptible(&(devc->queue2));
    }
    up(&devc->sem1);
	printk(KERN_INFO "Releasing device\n");
    return 0;
}

static ssize_t e2_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_ALERT "Read function called in driver\n");
    struct e2_dev *devc = filp->private_data;
	ssize_t ret = 0;
	down_interruptible(&devc->sem1);
	if (devc->mode == MODE1) {
		up(&devc->sem1);
     	if (*f_pos + count > ramdisk_size) {
       		printk("Trying to read past end of buffer!\n");
            return ret;
       }
	   ret = count - copy_to_user(buf, devc->ramdisk, count);
	}
	else {
          if (*f_pos + count > ramdisk_size) {
             printk("Trying to read past end of buffer!\n");
             up(&devc->sem1);
             return ret;
          }
          ret = count - copy_to_user(buf, devc->ramdisk, count);
	  up(&devc->sem1);
	}
	return ret;
}


static ssize_t e2_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_ALERT "Write function called in driver\n");
    struct e2_dev *devc;
	ssize_t ret = 0;
	devc = filp->private_data;
	down_interruptible(&devc->sem1);
	if (devc->mode == MODE1) {
		up(&devc->sem1);
        if (*f_pos + count > ramdisk_size) {
            printk("Trying to read past end of buffer!\n");
            return ret;
        }
        ret = count - copy_from_user(devc->ramdisk, buf, count);
	}
	else {
        if (*f_pos + count > ramdisk_size) {
            printk("Trying to read past end of buffer!\n");
            up(&devc->sem1);
            return ret;
        }
        ret = count - copy_from_user(devc->ramdisk, buf, count);
		up(&devc->sem1);
	}
	return ret;
}

static long e2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk(KERN_INFO "IOCTL called\n");
    struct e2_dev *devc = filp->private_data;
	
	if (_IOC_TYPE(cmd) != CDRV_IOC_MAGIC) {
		pr_info("Invalid magic number\n");
		return -ENOTTY;
	}
	if ( (_IOC_NR(cmd) != 1) && (_IOC_NR(cmd) != 2) ) {
		pr_info("Invalid cmd\n");
		return -ENOTTY;
	}
	
	switch(cmd) {
		case E2_IOCMODE2:
				printk(KERN_INFO "Changing to mode2\n");
				down_interruptible(&(devc->sem1));
				if (devc->mode == MODE2) {
					up(&devc->sem1);
					printk(KERN_INFO "Already in mode2\n");
					break;
				}
				if (devc->count1 > 1) {
					while (devc->count1 > 1) {
						up(&devc->sem1);
					    wait_event_interruptible(devc->queue1, (devc->count1 == 1));
						down_interruptible(&devc->sem1);
					}
				}
				devc->mode = MODE2;
				printk(KERN_INFO "Mode changed to mode2\n");
                devc->count1--;
                devc->count2++;
				up(&devc->sem2);
				up(&devc->sem1);
				break;
				
		case E2_IOCMODE1:
				printk(KERN_INFO "Changing to mode1\n");
				down_interruptible(&devc->sem1);
				if (devc->mode == MODE1) {
				   up(&devc->sem1);
				   printk(KERN_INFO "Already in mode1\n");
				   break;
				}
				if (devc->count2 > 1) {
				   while (devc->count2 > 1) {
				       up(&devc->sem1);
					   printk(KERN_INFO "Waiting for all other threads to exit\n");
				       wait_event_interruptible(devc->queue2, (devc->count2 == 1));
					   //printk(KERN_INFO "All other exited, processing to change the mode to Mode1\n");
				       down_interruptible(&devc->sem1);
				   }
				}
				devc->mode = MODE1;
				printk(KERN_INFO "Mode changed to mode1\n");
                devc->count2--;
                devc->count1++;
				down(&devc->sem2);
				up(&devc->sem1);
				break;
				
				default :
					pr_info("Unrecognized ioctl command\n");
					return -1;
					break;	
	}
	return 0;
}

static const struct file_operations fops = { 
  .owner = THIS_MODULE,
	.read = e2_read,
	.write = e2_write,
	.open = e2_open,
	.release = e2_release,
	.unlocked_ioctl = e2_ioctl,
};

static int __init my_init (void) {

   int ret = 0;
   dev_t dev_no = MKDEV(majorNo, minorNo);
   ret = register_chrdev_region(dev_no, 1, MYDEV_NAME);
   if (ret<0) {
    	printk(KERN_ALERT "mycdrv: failed to reserve major number");
    	return ret;
   }
   cl = class_create(THIS_MODULE, MYDEV_NAME);
   dev = kmalloc(sizeof(struct e2_dev), GFP_KERNEL);
   dev->ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
   memset(dev->ramdisk,0,ramdisk_size);
   cdev_init(&(dev->cdev), &fops); 
   dev->count1 = 0;
   dev->count2 = 0;
   dev->mode = MODE1;
   init_waitqueue_head(&dev->queue1);
   init_waitqueue_head(&dev->queue2);
   sema_init(&dev->sem1, 1);
   sema_init(&dev->sem2, 1);
   ret = cdev_add(&dev->cdev, dev_no, 1);
   if(ret < 0 ) {
      printk(KERN_INFO "Unable to register cdev");
      return ret;
   }
   device_create(cl, NULL, dev_no, NULL, MYDEV_NAME);
   return 0;
}

static void __exit my_exit(void) {

   dev_t devNo = MKDEV(majorNo, minorNo);  
   printk(KERN_INFO "cleanup: unloading driver\n");
   cdev_del(&(dev->cdev));
   kfree(dev->ramdisk);
   device_destroy(cl, devNo);
   kfree(dev);
   unregister_chrdev_region(devNo, 1);
   class_destroy(cl);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Assignment5");
MODULE_LICENSE("GPL v2");

