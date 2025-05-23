#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#define FPGA_HEX_BASE 0xff233000 // Adapt this address if needed
#define DIGIT_COUNT   6

static void __iomem *hex_base = NULL;
static uint8_t szMask[] = {
    63, 6, 91, 79, 102, 109, 125, 7,
    127, 111, 119, 124, 57, 94, 121, 113
};

static int __init hex_init(void)
{
    int i;
    hex_base = ioremap(FPGA_HEX_BASE, DIGIT_COUNT * sizeof(u32));
    if (!hex_base) {
        pr_err("hex: ioremap failed\n");
        return -ENOMEM;
    }
    for (i = 0; i < DIGIT_COUNT; i++) {
        iowrite32(szMask[i + 1], hex_base + 4 * i); // Affiche 1,2,3,4,5,6
    }
    pr_info("hex: module loaded\n");
    return 0;
}

static void __exit hex_exit(void)
{
    if (hex_base)
        iounmap(hex_base);
    pr_info("hex: module unloaded\n");
}

module_init(hex_init);
module_exit(hex_exit);
MODULE_LICENSE("GPL");