
Part 1:
 A Linux kernel module to enable user-space device interface for HC-SR04 sensors

In this assignment we have developed a loadable kernel module which initiates the devices depending on the input at the time of loading the module.

Command line run: make all

After the command "make all" hcsr.ko and hcsr files are gernerated.Load these files onto the board.

Run these files on the galileo serial terminal by running the following command sudo screen /dev/ttyUSB0 115200

inside the screen terminal enter:

1) insmod hcsr.ko number_of_device=2 
2) chmod 777 hcsr 

While running the driver programe , First time run using following command

3)./hcsr ioctl  (This is to configure the devices with pins and number of samples)

For all the consecutive run use following command

4)./hcsr
