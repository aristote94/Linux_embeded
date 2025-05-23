#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>

#define LED_COUNT 8
static int speed = 500; // ms, valeur par défaut
static int dir = 1;     // 1 = droite, -1 = gauche
static u8 pattern = 1;  // pattern courant

module_param(speed, int, 0644);
MODULE_PARM_DESC(speed, "Vitesse du chenillard en ms");
module_param(dir, int, 0644);
MODULE_PARM_DESC(dir, "Sens du chenillard (1=droite, -1=gauche)");

static struct timer_list chenillard_timer;
static struct proc_dir_entry *ensea_dir, *speed_entry, *dir_entry;

// Prototypes
static int leds_probe(struct platform_device *pdev);
static int leds_remove(struct platform_device *pdev);
static ssize_t leds_read(struct file *file, char *buffer, size_t len, loff_t *offset);
static ssize_t leds_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static ssize_t speed_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t speed_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t dir_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t dir_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

// An instance of this structure will be created for every ensea_led IP in the system
struct ensea_leds_dev {
    struct miscdevice miscdev;
    void __iomem *regs;
    u8 leds_value;
};

// Specify which device tree devices this driver supports
static struct of_device_id ensea_leds_dt_ids[] = {
    {
        .compatible = "dev,ensea"
    },
    { /* end of table */ }
};

// Inform the kernel about the devices this driver supports
MODULE_DEVICE_TABLE(of, ensea_leds_dt_ids);

// Data structure that links the probe and remove functions with our driver
static struct platform_driver leds_platform = {
    .probe = leds_probe,
    .remove = leds_remove,
    .driver = {
        .name = "Ensea LEDs Driver",
        .owner = THIS_MODULE,
        .of_match_table = ensea_leds_dt_ids
    }
};

// The file operations that can be performed on the ensea_leds character file
static const struct file_operations ensea_leds_fops = {
    .owner = THIS_MODULE,
    .read = leds_read,
    .write = leds_write
};

static struct file_operations speed_fops = {
    .owner = THIS_MODULE,
    .read = speed_read,
    .write = speed_write,
};

static struct file_operations dir_fops = {
    .owner = THIS_MODULE,
    .read = dir_read,
    .write = dir_write,
};

// Called when the driver is installed
static int leds_init(void)
{
    int ret_val = 0;
    pr_info("start init\n");

    // Register our driver with the "Platform Driver" bus
    ret_val = platform_driver_register(&leds_platform);
    if(ret_val != 0) {
        pr_err("platform_driver_register returned %d\n", ret_val);
        return ret_val;
    }

    pr_info("Ensea LEDs init sucess\n");

    return 0;
}
static struct ensea_leds_dev *global_led_dev = NULL; // Pour accéder au device dans le timer

static void chenillard_callback(struct timer_list *t) {
    // Met à jour le pattern selon le sens
    if (dir == 1)
        pattern = ((pattern << 1) | (pattern >> (LED_COUNT - 1))) & ((1 << LED_COUNT) - 1);
    else
        pattern = ((pattern >> 1) | ((pattern & 1) << (LED_COUNT - 1))) & ((1 << LED_COUNT) - 1);

    if (global_led_dev) {
        global_led_dev->leds_value = pattern;
        iowrite32(pattern, global_led_dev->regs);
    }
    mod_timer(&chenillard_timer, jiffies + msecs_to_jiffies(speed));
}


// Called whenever the kernel finds a new device that our driver can handle
// (In our case, this should only get called for the one instantiation of the Ensea LEDs module)
static int leds_probe(struct platform_device *pdev)
{
    int ret_val = -EBUSY;
    struct ensea_leds_dev *dev;
    struct resource *r = 0;

    pr_info("leds_probe enter\n");

    // Get the memory resources for this LED device
    r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(r == NULL) {
        pr_err("IORESOURCE_MEM (register space) does not exist\n");
        goto bad_exit_return;
    }

    // Create structure to hold device-specific information (like the registers)
    dev = devm_kzalloc(&pdev->dev, sizeof(struct ensea_leds_dev), GFP_KERNEL);

    // Both request and ioremap a memory region
    // This makes sure nobody else can grab this memory region
    // as well as moving it into our address space so we can actually use it
    dev->regs = devm_ioremap_resource(&pdev->dev, r);
    if(IS_ERR(dev->regs))
        goto bad_ioremap;

    // Turn the LEDs on (access the 0th register in the ensea LEDs module)
    dev->leds_value = 0x01;
    iowrite32(dev->leds_value, dev->regs);
    pattern = dev->leds_value;

    // Initialize the misc device (this is used to create a character file in userspace)
    dev->miscdev.minor = MISC_DYNAMIC_MINOR;    // Dynamically choose a minor number
    dev->miscdev.name = "ensea_leds";
    dev->miscdev.fops = &ensea_leds_fops;

    ret_val = misc_register(&dev->miscdev);
    if(ret_val != 0) {
        pr_info("Couldn't register misc device :(");
        goto bad_exit_return;
    }

    // Give a pointer to the instance-specific data to the generic platform_device structure
    // so we can access this data later on (for instance, in the read and write functions)
    platform_set_drvdata(pdev, (void*)dev);

    // Initialize the global device pointer
    global_led_dev = dev;
    pattern = dev->leds_value;

    // Crée /proc/ensea et ses fichiers
    ensea_dir = proc_mkdir("ensea", NULL);
    speed_entry = proc_create("speed", 0666, ensea_dir, &speed_fops);
    dir_entry = proc_create("dir", 0666, ensea_dir, &dir_fops);

    // Timer chenillard
    setup_timer(&chenillard_timer, chenillard_callback, 0);
    mod_timer(&chenillard_timer, jiffies + msecs_to_jiffies(speed));

    pr_info("leds_probe exit\n");

    return 0;

bad_ioremap:
   ret_val = PTR_ERR(dev->regs);
bad_exit_return:
    pr_info("leds_probe bad exit :(\n");
    return ret_val;
}



// This function gets called whenever a read operation occurs on one of the character files
static ssize_t leds_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{
    int success = 0;

    /*
    * Get the ensea_leds_dev structure out of the miscdevice structure.
    *
    * Remember, the Misc subsystem has a default "open" function that will set
    * "file"s private data to the appropriate miscdevice structure. We then use the
    * container_of macro to get the structure that miscdevice is stored inside of (which
    * is our ensea_leds_dev structure that has the current led value).
    *
    * For more info on how container_of works, check out:
    * http://linuxwell.com/2012/11/10/magical-container_of-macro/
    */
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);

    // Give the user the current led value
    u8 val = pattern;
    success = copy_to_user(buffer, &val, sizeof(val));

    // If we failed to copy the value to userspace, display an error message
    if(success != 0) {
        pr_info("Failed to return current led value to userspace\n");
        return -EFAULT; // Bad address error value. It's likely that "buffer" doesn't point to a good address
    }

    return 0; // "0" indicates End of File, aka, it tells the user process to stop reading
}

// This function gets called whenever a write operation occurs on one of the character files
static ssize_t leds_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{
    int success = 0;

    /*
    * Get the ensea_leds_dev structure out of the miscdevice structure.
    *
    * Remember, the Misc subsystem has a default "open" function that will set
    * "file"s private data to the appropriate miscdevice structure. We then use the
    * container_of macro to get the structure that miscdevice is stored inside of (which
    * is our ensea_leds_dev structure that has the current led value).
    *
    * For more info on how container_of works, check out:
    * http://linuxwell.com/2012/11/10/magical-container_of-macro/
    */
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);

    // Get the new led value (this is just the first byte of the given data)
    u8 val;
    success = copy_from_user(&val, buffer, sizeof(val));
    if(success != 0) return -EFAULT;
    pattern = val & ((1 << LED_COUNT) - 1);
    dev->leds_value = pattern;
    iowrite32(pattern, dev->regs);
    return len;
}

static ssize_t speed_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char msg[16];
    int len = snprintf(msg, sizeof(msg), "%d\n", speed);
    return simple_read_from_buffer(buf, count, ppos, msg, len);
}

static ssize_t speed_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char kbuf[16];
    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';
    kstrtoint(kbuf, 10, &speed);
    return count;
}

static ssize_t dir_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char msg[8];
    int len = snprintf(msg, sizeof(msg), "%d\n", dir);
    return simple_read_from_buffer(buf, count, ppos, msg, len);
}

static ssize_t dir_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char kbuf[8];
    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';
    kstrtoint(kbuf, 10, &dir);
    return count;
}

// Gets called whenever a device this driver handles is removed.
// This will also get called for each device being handled when
// our driver gets removed from the system (using the rmmod command).
static int leds_remove(struct platform_device *pdev)
{
    // Grab the instance-specific information out of the platform device
    struct ensea_leds_dev *dev = (struct ensea_leds_dev*)platform_get_drvdata(pdev);

    pr_info("leds_remove enter\n");

    // Turn the LEDs off
    iowrite32(0x00, dev->regs);

    // Unregister the character file (remove it from /dev)
    misc_deregister(&dev->miscdev);

    // Supprime les entrées dans /proc
    del_timer_sync(&chenillard_timer);
    if (speed_entry) remove_proc_entry("speed", ensea_dir);
    if (dir_entry) remove_proc_entry("dir", ensea_dir);
    if (ensea_dir) remove_proc_entry("ensea", NULL);
    global_led_dev = NULL;

    pr_info("leds_remove exit\n");

    return 0;
}

// Called when the driver is removed
static void leds_exit(void)
{
    pr_info("Ensea LEDs module exit\n");

    // Unregister our driver from the "Platform Driver" bus
    // This will cause "leds_remove" to be called for each connected device
    platform_driver_unregister(&leds_platform);

    pr_info("Ensea LEDs module successfully unregistered\n");
}

// Tell the kernel which functions are the initialization and exit functions
module_init(leds_init);
module_exit(leds_exit);

// Define information about this kernel module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Devon Andrade <devon.andrade@oit.edu>");
MODULE_DESCRIPTION("Exposes a character device to user space that lets users turn LEDs on and off");
MODULE_VERSION("1.0");
