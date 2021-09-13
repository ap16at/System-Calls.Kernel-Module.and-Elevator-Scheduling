#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/time.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_LEN 100 // max length of read/write message

static struct proc_dir_entry* proc_entry;	//pointer to proc entry

static char *msg;
static char *msg2;
static int procfs_buf_len;	//variable to hold length of message

struct timespec time;

int repetition = 0;
long seconds;
long nanoSeconds;
long tempSec;
long tempNSec;

int procfile_open(struct inode *inode, struct file * file)
{
	printk(KERN_INFO "proc_open\n");
	
	msg = kmalloc(sizeof(char) * BUF_LEN, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	msg2 = kmalloc(sizeof(char) * BUF_LEN, __GFP_RECLAIM | __GFP_IO | __GFP_FS);

	repetition++;

	if(msg == NULL)
	{	
		printk(KERN_WARNING "Error occured when allocating memory");
		return -ENOMEM;
	}

	time = current_kernel_time();
	sprintf(msg, "current time: %ld.%ld\n", time.tv_sec, time.tv_nsec);

	if(repetition > 1)
	{
		seconds = time.tv_sec - tempSec;
		nanoSeconds = time.tv_nsec - tempNSec;
		if(nanoSeconds < 0)
		{
			seconds = seconds - 1;
			nanoSeconds = nanoSeconds + 1000000000;
		}
		sprintf(msg2, "elapsed time: %ld.%ld\n", seconds, nanoSeconds);
		strcat(msg, msg2);
	}

	tempSec = time.tv_sec;
	tempNSec = time.tv_nsec;

	return 0;

}

static ssize_t procfile_read(struct file *file, char * ubuf, size_t count, loff_t * ppos)
{
	printk(KERN_INFO "proc_read\n");
	procfs_buf_len = strlen(msg);

	if(*ppos > 0 || count < procfs_buf_len)
		return 0;

	if(copy_to_user(ubuf, msg, procfs_buf_len))
		return -EFAULT;

	*ppos = procfs_buf_len;

	printk(KERN_INFO "gave to user%s\n", msg);

	return procfs_buf_len;
}

int procfile_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "proc_release\n");
	
	kfree(msg);
	kfree(msg2);
	
	return 0;
}

static struct file_operations procfile_fops = {
	.owner = THIS_MODULE,
	.open = procfile_open,
	.read = procfile_read,
	.release = procfile_release,
};

static int hello_init(void)
{
	proc_entry = proc_create("timer", 0666, NULL, &procfile_fops);

	if(proc_entry == NULL)
		return -ENOMEM;

	return 0;
}


static void hello_exit(void)
{
	proc_remove(proc_entry);
	return;
}

module_init(hello_init);
module_exit(hello_exit);

