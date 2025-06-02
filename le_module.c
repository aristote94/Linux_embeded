#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
#define MAJOR_NUM 240

static char kernel_buffer[256];

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char msg[] = "Hello depuis /proc/le_module\n";
    return simple_read_from_buffer(buf, count, ppos, msg, sizeof(msg) - 1);
}

static struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = proc_read,
};

#define DEFAULT_SPEED 500 // ms
#define LED_COUNT 8

static int speed = DEFAULT_SPEED;
static int dir = 1; // 1 = droite, -1 = gauche
static int pattern = 1;
module_param(speed, int, 0644);
MODULE_PARM_DESC(speed, "Vitesse du chenillard en ms");
module_param(dir, int, 0644);
MODULE_PARM_DESC(dir, "Sens du chenillard (1=droite, -1=gauche)");

// Fonctions pour /proc/ensea/speed
static ssize_t speed_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char msg[32];
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
static struct file_operations speed_fops = {
    .owner = THIS_MODULE,
    .read = speed_read,
    .write = speed_write,
};

// Fonctions pour /proc/ensea/dir
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
static struct file_operations dir_fops = {
    .owner = THIS_MODULE,
    .read = dir_read,
    .write = dir_write,
};

// Fonctions pour /dev/ensea-led
static ssize_t led_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char msg[16];
    int len = snprintf(msg, sizeof(msg), "%d\n", pattern);
    return simple_read_from_buffer(buf, count, ppos, msg, len);
}
static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char kbuf[8];
    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';
    kstrtoint(kbuf, 10, &pattern);
    return count;
}
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .read = led_read,
    .write = led_write,
};

// Timer callback pour le chenillard
static struct timer_list chenillard_timer;
static void chenillard_callback(struct timer_list *t) {
    // Mettre à jour le pattern selon le sens
    if (dir == 1)
        pattern = (pattern << 1) | (pattern >> (LED_COUNT - 1));
    else
        pattern = (pattern >> 1) | ((pattern & 1) << (LED_COUNT - 1));
    // Ici, ajouter le code pour afficher le pattern sur le matériel si besoin
    mod_timer(&chenillard_timer, jiffies + msecs_to_jiffies(speed));
}

// Initialisation du module
static int __init mon_module_init(void) {
    int result;
    // Création des entrées /proc/ensea/speed et /proc/ensea/dir
    struct proc_dir_entry *ensea_dir = proc_mkdir("ensea", NULL);
    if (!ensea_dir) return -ENOMEM;
    proc_create("speed", 0666, ensea_dir, &speed_fops);
    proc_create("dir", 0666, ensea_dir, &dir_fops);

    // Enregistrement du device /dev/ensea-led
    result = register_chrdev(MAJOR_NUM, "ensea-led", &led_fops);
    if (result < 0) return result;

    // Timer
    timer_setup(&chenillard_timer, chenillard_callback, 0);
    mod_timer(&chenillard_timer, jiffies + msecs_to_jiffies(speed));
    return 0;
}

// Nettoyage du module
static void __exit mon_module_exit(void) {
    del_timer_sync(&chenillard_timer);
    remove_proc_entry("speed", proc_mkdir("ensea", NULL));
    remove_proc_entry("dir", proc_mkdir("ensea", NULL));
    remove_proc_entry("ensea", NULL);
    unregister_chrdev(MAJOR_NUM, "ensea-led");
}

module_init(mon_module_init);
module_exit(mon_module_exit);