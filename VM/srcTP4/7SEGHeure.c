#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <linux/timekeeping.h>

#define FPGA_HEX_BASE 0xff233000 // Adresse à adapter si besoin
#define DIGIT_COUNT   6

static void __iomem *hex_base = NULL;
static struct timer_list hex_timer;

static uint8_t szMask[] = {
    63, 6, 91, 79, 102, 109, 125, 7,
    127, 111, 119, 124, 57, 94, 121, 113
};

static void hex_display_time(struct timer_list *t)
{
    struct timespec64 ts;
    struct tm tm;
    int digits[DIGIT_COUNT];
    int i;

    ktime_get_real_ts64(&ts);
    time_to_tm(ts.tv_sec, 0, &tm);

    // HHMMSS sur 6 digits
    digits[0] = tm.tm_hour / 10;
    digits[1] = tm.tm_hour % 10;
    digits[2] = tm.tm_min / 10;
    digits[3] = tm.tm_min % 10;
    digits[4] = tm.tm_sec / 10;
    digits[5] = tm.tm_sec % 10;

    // Affichage miroir : digit 0 (heure dizaine) sur le digit le plus à gauche (5)
    for (i = 0; i < DIGIT_COUNT; i++) {
        iowrite32(szMask[digits[i]], hex_base + 4 * (DIGIT_COUNT - 1 - i));
    }

    mod_timer(&hex_timer, jiffies + HZ); // Relance dans 1 seconde
}

static int __init hex_init(void)
{
    hex_base = ioremap(FPGA_HEX_BASE, DIGIT_COUNT * sizeof(u32));
    if (!hex_base) {
        pr_err("hex: ioremap failed\n");
        return -ENOMEM;
    }

    setup_timer(&hex_timer, hex_display_time, 0);
    mod_timer(&hex_timer, jiffies + HZ); // Démarre le timer

    pr_info("hex: module loaded (affichage heure)\n");
    return 0;
}

static void __exit hex_exit(void)
{
    del_timer_sync(&hex_timer);
    if (hex_base)
        iounmap(hex_base);
    pr_info("hex: module unloaded\n");
}

module_init(hex_init);
module_exit(hex_exit);
MODULE_LICENSE("GPL");