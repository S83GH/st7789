# DRM драйвер для дисплея ST7789 (1.9", 170×320, SPI)

Установка:
В системе должны быть установлен пакет h-файлов от текущего ядра.  
Если вы собирали ядро, то они у вас есть и как собирать модуль ядра вы должны знать.

git clone https://github.com/S83GH/st7789  
cd st7789  
make  
make install

reboot

После перезагрузки должен появится файл видеокарты /dev/dri/card0 

Пример оверлея  

``` 
/dts-v1/;
/plugin/;

/ {
	compatible = "allwinner,sun8i-t113s", "allwinner,sun8i-r528";
};

&spi1
	{
	#address-cells = <1>;
	#size-cells = <0>;
	status = "okay";
	pinctrl-0 = <&spi1_3pins_PD>;
	pinctrl-names = "default";

	spi1_display: display@0
		{
		compatible = "sitronix,st7789v-170x320";
		
		reg	= <0>;
		backlight = <&backlight_pwm5>; //Подсветка - не обязательно, можно закоментировать
		
		//Частота SPI
		//60MHz(китайский 1.9 дисплей с aliexpress подключенный на длинных проводах)
		spi-max-frequency = <60000000>; 
		
		dc-gpios = <&pio 3 13 0>; //PD13
		reset-gpios = <&pio 3 14 0>; //PD14
		
		buswidth = <8>;
		rotation = <270>; //[0-портрет1] [180-портрет2]  [90-не портрет чип дисплея слево] [270-не портрет чип дисплея справа]
		status	 = "okay";
		};
	};
```
