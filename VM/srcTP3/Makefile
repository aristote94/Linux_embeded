obj-m += gpio_leds.o
CFLAGS_MODULE = -fno-pic
all:
	$(MAKE) -C /home/ensea/linux-socfpga M=$(PWD) modules

clean:
	$(MAKE) -C /home/ensea/linux-socfpga M=$(PWD) clean