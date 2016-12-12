/* CSci5103 Fall 2016
* Assignment# 1
* name: Ajay Sundar Karuppasamy, Manu Khandelwal
* student id: 5298653, 5272109
* x500 id: karup002, khand055
* CSELABS machine: csel-x13-umh.cselabs.umn.edu
*/

/*
 * pipe.c -- fifo driver for scullbuffer
 *
 * Copyright (C) 2016 Ajay Sundar Karuppasamy and Manu Khandelwal
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form. We duly attribute to
 * the works of Alessandro Rubini and Jonathan Corbet, authors of 
 * "Linux Device Drivers", based on which large part of this code 
 * has been influenced. No warranty is attached; we cannot take 
 * responsibility for errors or fitness for use.
 *
 */

/*
 * Please stick this banner as required by the original implementers
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

/**********************************************************************************************
 * Requirements:
 *  - to implement scullbuffer character device driver
 *    	* buffer needs to hold items of size 32 bytes each
 *  - to accept input parameters to specify number of items to contain in buffer
 *    	* implement as a module parameter
 *  - to write scripts to load and unload the device driver
 *  - to implement open(), release(), read() and write() functions
 *
 * Attributions:
 *  Our implementation of scullbuffer is largely influenced by the text and sample codes
 *  from the book "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 *       by O'Reilly & Associates.
 *    	* reuse of the scullpipe driver code, tailoring it for the assignment as needed
 *		* reuse of the load and unload scripts for the scull driver module
 *
 **********************************************************************************************/
 
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "scull.h"		/* local definitions */

#define init_MUTEX(_m) sema_init(_m, 1);


struct scull_pipe {
        wait_queue_head_t inq, outq;       /* read and write queues */
        char *buffer, *end;                /* begin of buf, end of buf */
        int buffersize;                    /* used in pointer arithmetic */
        char *rp, *wp;                     /* where to read, where to write */
        int nreaders, nwriters;            /* number of openings for r/w */
        struct semaphore sem;              /* mutual exclusion semaphore */
        struct cdev cdev;                  /* Char device structure */
		int space_remaining;
};

/* parameters */
static int scull_p_nr_devs = SCULL_P_NR_DEVS;	/* number of pipe devices */
int scull_p_buffer =  SCULL_P_BUFFER;	/* buffer size */
int scull_p_num_buffer =  NUM_OF_ITEM;	/* buffer size */

dev_t scull_p_devno;			/* Our first device number */

module_param(scull_p_nr_devs, int, 0);	/* FIXME check perms */
module_param(scull_p_buffer, int, 0);
module_param(scull_p_num_buffer, int, 0);

static struct scull_pipe *scull_p_devices;

static int spacefree(struct scull_pipe *dev);
/*
 * Open and close
 */
static int scull_p_open(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev;
	int scull_size = scull_p_num_buffer * SIZE_OF_ITEM;
	dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
	filp->private_data = dev;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if (!dev->buffer) {
		/* allocate the buffer */
		PDEBUG("Allocating the memory %d for the buffer", scull_size);
		dev->buffer = kmalloc(scull_size, GFP_KERNEL);
		if (!dev->buffer) {
			up(&dev->sem);
			return -ENOMEM;
		}
		dev->space_remaining = scull_p_num_buffer;
		dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */
	}
	
	dev->buffersize = scull_size;
	dev->end = dev->buffer + dev->buffersize;
	if (filp->f_mode & FMODE_WRITE) { //producer
		++(dev->nwriters);
	}
	else if (filp->f_mode & FMODE_READ) { // consumer
		++(dev->nreaders);
	}

	up(&dev->sem);

	return nonseekable_open(inode, filp);
}

static int scull_p_release(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev = filp->private_data;
	int wake_consumer=false, wake_producer=false;
	down(&dev->sem);
	if ( (filp->f_mode & FMODE_READ) & !(filp->f_mode & FMODE_WRITE) )
	{
	    PDEBUG("DEBUG: consumer process is releasing device \n");
		dev->nreaders--;
	}
	if (filp->f_mode & FMODE_WRITE)
	{
	    PDEBUG("DEBUG: producer process releasing device \n");
		dev->nwriters--;
	}
	if (dev->nreaders + dev->nwriters == 0) {
		PDEBUG("DEBUG: no more processes; freeing up buffer \n");
		kfree(dev->buffer);
		dev->buffer = NULL; /* the other fields are not checked on open */
	}
	else if ( dev->nreaders == 0 )
    {
        // if there are no consumers, signal any sleeping producers
	    wake_producer = true;
    }
	else if ( dev->nwriters == 0 )
    {
        // if there are no producers, signal any sleeping consumers
	    wake_consumer = true;
    }
	up(&dev->sem);
	
	if (wake_producer)
	{
	    // if no consumers, signal any sleeping producers
	    PDEBUG("DEBUG: no consumers, wakeup sleeping producers \n");
	    wake_up_interruptible(&dev->outq);
	}
    if (wake_consumer)
    {
        // if no producers, signal any sleeping consumers
        PDEBUG("DEBUG: no producers, wakeup sleeping consumers \n");
        wake_up_interruptible(&dev->inq);
    }

	return 0;
}

/*
 * Data management: read and write
*/
static ssize_t scull_p_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_pipe *dev = filp->private_data;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;


	while (spacefree(dev) >= scull_p_num_buffer) { /* nothing to read */
		up(&dev->sem); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
				
		if ( dev->nwriters == 0 ) /* empty buffer and no producers to populate */
		    return 0; 
			
		PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (copy_to_user(buf, dev->rp, SIZE_OF_ITEM)) {
		up (&dev->sem);
		return -EFAULT;
	}
	dev->rp += SIZE_OF_ITEM;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	dev->space_remaining++;
	
	up (&dev->sem);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	PDEBUG("Skull Driver read data: %s  of size : %d  for process : %s", buf, SIZE_OF_ITEM, current->comm);

	return ((ssize_t)strlen(buf));
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		DEFINE_WAIT(wait);
		
		up(&dev->sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		
		if(dev->nreaders == 0){
			PDEBUG("Returning 0 to the user process \"%s\" as buffer is full and no waiting reader",current->comm);
			return BUFFER_FULL_NO_READER;
		} else {
			PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
		}
		
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		
		// awake but there are no consumers
		if ( dev->nreaders == 0 )
            return BUFFER_FULL_NO_READER;

		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	return 0;
}	

/* How much space is free? */
static int spacefree(struct scull_pipe *dev)
{
	return dev->space_remaining;
}

static ssize_t scull_p_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_pipe *dev = filp->private_data;
	int result;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = scull_getwritespace(dev, filp);
	if (result) {
		if (result == BUFFER_FULL_NO_READER) 
			return -1;
		else 
			return result;/* scull_getwritespace called up(&dev->sem) */
	}

	/* ok, space is there, accept something */
	if (copy_from_user(dev->wp, buf, SIZE_OF_ITEM)) {
		up (&dev->sem);
		return -EFAULT;
	}
	dev->wp += SIZE_OF_ITEM;
	if (dev->wp == dev->end)
		dev->wp = dev->buffer; /* wrapped */
	dev->space_remaining--;
	up(&dev->sem);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */
	PDEBUG("Skull Driver write data: %s  of size : %d  for process : %s", buf, SIZE_OF_ITEM, current->comm);
	return SIZE_OF_ITEM;
}

static unsigned int scull_p_poll(struct file *filp, poll_table *wait)
{
	struct scull_pipe *dev = filp->private_data;
	unsigned int mask = 0;

	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	down(&dev->sem);
	poll_wait(filp, &dev->inq,  wait);
	poll_wait(filp, &dev->outq, wait);
	if (dev->rp != dev->wp)
		mask |= POLLIN | POLLRDNORM;	/* readable */
	if (spacefree(dev))
		mask |= POLLOUT | POLLWRNORM;	/* writable */
	up(&dev->sem);
	return mask;
}

/*
 * The file operations for the pipe device
 * (some are overlayed with bare scull)
 */
struct file_operations scull_pipe_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		scull_p_read,
	.write =	scull_p_write,
	.poll =		scull_p_poll,
	.open =		scull_p_open,
	.release =	scull_p_release,
};

/*
 * Set up a cdev entry.
 */
static void scull_p_setup_cdev(struct scull_pipe *dev, int index)
{
	int err, devno = scull_p_devno + index;
    
	cdev_init(&dev->cdev, &scull_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

/*
 * Initialize the pipe devs; return how many we did.
 */
int scull_p_init(dev_t firstdev)
{
	int i, result;

	result = register_chrdev_region(firstdev, scull_p_nr_devs, "scullp");
	if (result < 0) {
		printk(KERN_NOTICE "Unable to get scullp region, error %d\n", result);
		return 0;
	}
	scull_p_devno = firstdev;
	scull_p_devices = kmalloc(scull_p_nr_devs * sizeof(struct scull_pipe), GFP_KERNEL);
	if (scull_p_devices == NULL) {
		unregister_chrdev_region(firstdev, scull_p_nr_devs);
		return 0;
	}
	memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(struct scull_pipe));
	for (i = 0; i < scull_p_nr_devs; i++) {
		init_waitqueue_head(&(scull_p_devices[i].inq));
		init_waitqueue_head(&(scull_p_devices[i].outq));
		init_MUTEX(&scull_p_devices[i].sem);
		scull_p_setup_cdev(scull_p_devices + i, i);
	}
	return scull_p_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_p_cleanup(void)
{
	int i;

	if (!scull_p_devices)
		return; /* nothing else to release */

	for (i = 0; i < scull_p_nr_devs; i++) {
		cdev_del(&scull_p_devices[i].cdev);
		kfree(scull_p_devices[i].buffer);
	}
	kfree(scull_p_devices);
	unregister_chrdev_region(scull_p_devno, scull_p_nr_devs);
	scull_p_devices = NULL; /* pedantic */
}
