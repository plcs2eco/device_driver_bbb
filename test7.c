#include <linux/module.h>  /* MODULE macro, THIS_MODULE */
#include <linux/kernel.h>  /* printk(), container_of() */
//#include <linux/proc_fs.h> /* alloc_chrdev_region(), unregister_chrdev_region() */
#include <linux/fs.h> /* alloc_chrdev_region(), unregister_chrdev_region() */
#include <linux/cdev.h>    /* cdev_int(), cdev_add(), cdev_del() */
#include <asm/uaccess.h>   /* copy_from_user(), copy_to_user() */
#include <linux/slab.h>    /* kmalloc(), kfree() */
#include <linux/device.h>  /* class_create(), device_create() */
#include <linux/kdev_t.h>  /* MAJOR(), MINOR(), MKDEV() */
#include <linux/err.h>     /* IS_ERR(), PTR_ERR(), ERR_PTR()*/

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

#define DRIVER_NAME "my_gpio_driver4"
#define NO_OF_DEVICES 4
#define MEM_SIZE_DEV0 1024 /* バッファサイズ */
#define MEM_SIZE_DEV1 1024 /* バッファサイズ */
#define MEM_SIZE_DEV2 512  /* バッファサイズ */
#define MEM_SIZE_DEV3 1024 /* バッファサイズ */

#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR   0x11

static char my_buffer_dev0[MEM_SIZE_DEV0];
static char my_buffer_dev1[MEM_SIZE_DEV1];
static char my_buffer_dev2[MEM_SIZE_DEV2];
static char my_buffer_dev3[MEM_SIZE_DEV3];
/* Device private data struct*/
static struct mydev_private_data
{
    char *buffer;
    unsigned int size;
    const char *serial_number;
    int perm;
    struct cdev cdev;
};

/* Driver private data struct*/
static struct mydrv_private_data
{
    int total_devices;
    dev_t dev_id; /* デバイスのメジャー番号とマイナー番号保持用 */
    struct class *device_myclass;
    struct device *device_mydevice;
    struct mydev_private_data mydev_data[NO_OF_DEVICES];
};

static struct mydrv_private_data mydrv_data = 
{
    .total_devices = NO_OF_DEVICES,
    .mydev_data = {
        [0] = {
            .buffer = my_buffer_dev0,
            .size = MEM_SIZE_DEV0,
            .serial_number = "MYGPIOx0123232",
            .perm = RDONLY
        },
        [1] = {
            .buffer = my_buffer_dev1, .size = MEM_SIZE_DEV1, .serial_number = "MYGPIOx19gt7u", .perm = WRONLY
        },
        [2] = {
            .buffer = my_buffer_dev2, .size = MEM_SIZE_DEV2, .serial_number = "MYGPIOx2ghtyiolrm123232", .perm = RDWR
        },
        [3] = {
            .buffer = my_buffer_dev3, .size = MEM_SIZE_DEV3, .serial_number = "MYGPIOx355555123232", .perm = RDWR
        }}};

static int check_permission(int dev_perm, int acc_mode ){
    pr_info("dev_perm = %x\n", dev_perm);
    if(dev_perm == RDWR)
        pr_info("Read write dev_perm = %x\n", dev_perm);
        return 0;
    if((dev_perm == RDONLY) && ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE))) 
        pr_info("Read Only dev_perm = %x\n", dev_perm);
        return 0;
    if((dev_perm == WRONLY) && (!(acc_mode & FMODE_READ) && (acc_mode & FMODE_WRITE))) 
        pr_info("Write Only dev_perm = %x\n", dev_perm);
        return 0;

    return -EPERM;
}


static int my_gpio_open(struct inode *inode, struct file *filp)
{
    int ret;
    int minor_n;
    struct mydev_private_data *mydev_data;
    minor_n = MINOR(inode->i_rdev);
    pr_info("cdev_open minor = %d]\n", minor_n);

    /* get device's private datastructure*/
    mydev_data = container_of(inode->i_cdev, struct mydev_private_data, cdev);
    filp->private_data = mydev_data;

    ret = check_permission(mydev_data->perm, filp->f_mode);

	(!ret)?pr_info("open was successful ret = %d\n", ret):pr_info("open was unsuccessful ret = %d\n", ret);
    pr_info("cdev_open[mydev_data from container_of = %x]\n", mydev_data);

    return 0;
}
static int my_gpio_release(struct inode *inode, struct file *filp)
{
    pr_info("cdev_release successfully done\n");

    return 0;
}
static ssize_t my_gpio_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct mydev_private_data *mydev_data = (struct mydev_private_data*)filp->private_data;
    int max_size = mydev_data->size;
    pr_info("myDevice_read requested for %zu bytes, current file position = %lld\n", count, *f_pos);

    if ((*f_pos + count) > max_size)
        count = max_size - *f_pos;

    if (copy_to_user(buf, mydev_data->buffer+(*f_pos), count) != 0)
    {
        return -EFAULT;
    }
    *f_pos += count;

    pr_info("myDevice_read done for %zu bytes, updated file position = %lld\n", count, *f_pos);

    return count;
}
static ssize_t my_gpio_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct mydev_private_data *mydev_data = (struct mydev_private_data*)filp->private_data;
    int max_size = mydev_data->size;
    int ret;
    pr_info("myDevice_write minor_no = %x, requested bytes = %zu , current file position = %lld\n", iminor(filp->f_path.dentry->d_inode), count, *f_pos);
    if((*f_pos + count) > max_size)
        count = max_size - *f_pos;

	if(!count){
		pr_err("No space left on the device \n");
		return -ENOMEM;
	}

    if(copy_from_user(mydev_data->buffer+(*f_pos), buf, count)){
        pr_err("ERROR:ret=%d\n",ret);
        return -EFAULT;
    }

    *f_pos += count;

    pr_info("myDevice_write done %zu bytes, updated file position = %lld\n",count, *f_pos);
    /* never return 0 for write */
    return count;
}

loff_t my_gpio_lseek(struct file *filp, loff_t offset, int whence)
{
    struct mydev_private_data *mydev_data = (struct mydev_private_data *)filp->private_data;
    int max_size = mydev_data->size;
    loff_t temp;

    pr_info("lseek CURRENT file position= %lld \n", filp->f_pos);
    switch(whence){
        case SEEK_SET:
            if(((offset > max_size) || (offset < 0)))

                return -EINVAL;
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + offset;
            if((temp > max_size || (temp < 0)))
                return -EINVAL;
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = max_size + offset;
            if(((temp > max_size) || (temp < 0)))
                return -EINVAL;
            filp->f_pos = temp;
            break;
        default:
            return -EINVAL;
    }
    pr_info("lseek NEW file position= %lld \n", filp->f_pos);
    return filp->f_pos;
}
/* file_operations構造体 */
static struct file_operations my_cdev_fops = {
    .owner = THIS_MODULE,
    .read = my_gpio_read,
    .write = my_gpio_write,
    .llseek = my_gpio_lseek,
    .open = my_gpio_open,
    .release = my_gpio_release,
    .unlocked_ioctl = NULL};

static int my_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

/* 初期化ルーチン */
static int __init init_my_gpio(void)
{
    int ret;
    int i;
    pr_info("ichiri*****init_my_gpio name=%s\n", DRIVER_NAME);
    /* キャラクタデバイスメジャー番号動的取得 */
    ret = alloc_chrdev_region(&mydrv_data.dev_id, 0, NO_OF_DEVICES, DRIVER_NAME);
    if (ret != 0)
    {
        pr_warn("alloc_chrdev_region failed. ret = %d\n", ret);
        goto out;
    }

    mydrv_data.device_myclass = class_create(THIS_MODULE, "MyDev_class");
    if (IS_ERR(mydrv_data.device_myclass))
    {
        ret = PTR_ERR(mydrv_data.device_myclass);
        pr_err("class_create failed. ret = %d\n", ret);
        goto unreg_chrdev;
    }
    // mydrv_data.device_myclass->dev_uevent = my_uevent;

    for (i = 0; i < NO_OF_DEVICES; i++)
    {
        pr_info("device number <major>:<minor> = <%d>:<%d>\n", MAJOR(mydrv_data.dev_id + i), MINOR(mydrv_data.dev_id + i));
        /* /sys/class/MyDev/MyDev0~4を作成 */
        cdev_init(&mydrv_data.mydev_data[i].cdev, &my_cdev_fops);
   		mydrv_data.mydev_data[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&mydrv_data.mydev_data[i].cdev, mydrv_data.dev_id + i, 1);
        // ret = cdev_add(&mydrv_data.mydev_data[i].cdev, &mydrv_data.dev_id + i, 1);
        if (ret != 0)
        {
            pr_err("cdev_add failed. ret = %d\n", ret);
            goto cdev_del;
        }

        mydrv_data.device_mydevice = device_create(mydrv_data.device_myclass, NULL, mydrv_data.dev_id + i, NULL, "MyDev%d", i);
        if (IS_ERR(mydrv_data.device_myclass))
        {
            ret = PTR_ERR(mydrv_data.device_mydevice);
            pr_err("device_create failed. ret = %d\n", ret);
            goto class_del;
        }
    }

    return 0;

cdev_del:
class_del:
    for (; i >=0; i--)
    // for (i=0; i < NO_OF_DEVICES; i++)
    {
        device_destroy(mydrv_data.device_myclass, mydrv_data.dev_id + i);
        cdev_del(&mydrv_data.mydev_data[i].cdev);
    }
    class_destroy(mydrv_data.device_myclass);
unreg_chrdev:
    unregister_chrdev_region(mydrv_data.dev_id, NO_OF_DEVICES);
out:
    return ret;
}
/* モジュール解放関数 */
static void __exit exit_my_gpio(void)
{
    int i;
    /* キャラクタデバイス削除 */
    for (i = 0; i < NO_OF_DEVICES; i++)
    {
        device_destroy(mydrv_data.device_myclass, mydrv_data.dev_id + i);
        cdev_del(&mydrv_data.mydev_data[i].cdev);
    }

    /* このデバイスのクラス登録を削除 (/sys/class/mydevice/を削除する) */
    class_destroy(mydrv_data.device_myclass);

    /* デバイス番号の解放 */
    unregister_chrdev_region(mydrv_data.dev_id, NO_OF_DEVICES);

    pr_info("exited my_gpio\n");
}

module_init(init_my_gpio);
module_exit(exit_my_gpio);

MODULE_DESCRIPTION(DRIVER_NAME);
MODULE_LICENSE("Dual BSD/GPL");