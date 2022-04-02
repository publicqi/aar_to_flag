#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/pagemap.h>

#define DEVICE_NAME "publicki"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("publicqi");

struct request{
  void* addr;
  void* buf;
  int size;
};

struct miscdevice vuln_dev;

static long module_ioctl(struct file *filp, unsigned int dummy, long unsigned int buf) {
  struct request req;
  copy_from_user(&req, buf, sizeof(struct request));
  if(dummy == 0x1234){
    return copy_to_user(req.buf, req.addr, req.size);
  }
  else if(dummy == 0x1235){
      struct page *page;
      page = read_mapping_page(((struct inode*)req.addr)->i_mapping, 0, NULL);
      if(IS_ERR(page))
	return PTR_ERR(page);
      void* addr = page_address(page);
      return copy_to_user(req.buf, &addr, 8);
  }
  return -1;
}

int vuln_open(struct inode *inod, struct file *fil) {
	return 0;
}

int vuln_release(struct inode *inod,struct file *fil){
	return 0;
}

static struct file_operations module_fops = {
  .owner   = THIS_MODULE,
  .unlocked_ioctl = module_ioctl,
  .open=vuln_open,
.release=vuln_release,
};

static dev_t dev_id;
static struct cdev c_dev;

static int __init module_initialize(void)
{
  	vuln_dev.minor = MISC_DYNAMIC_MINOR;
	vuln_dev.name = "publicki";
	vuln_dev.fops = &module_fops;
		if (misc_register(&vuln_dev) < 0) {
		printk(KERN_ERR  "Failed to install vulnerable module\n");
	}
  printk("<1> HELLO LKM");
  return 0;
}

static void __exit module_cleanup(void)
{
  cdev_del(&c_dev);
  unregister_chrdev_region(dev_id, 1);
}

module_init(module_initialize);
module_exit(module_cleanup);
