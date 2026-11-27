obj-m := lcd_st7789_19.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo cp lcd_st7789_19.ko /lib/modules/$(shell uname -r)/kernel/drivers/gpu/drm/panel
	sudo depmod -a
