Part 2: Platform device driver and sysfs interface for HC-SR04

In this part a loadable module enables driver operations for two  HC-SR04 sensors.

The following are the steps to test this module:

Run the Makefile to obtain the executable and the relocable file:
		make all

After the command "make all", .platform_device.ko, platform_driver.ko gernerated.Load these
along with  Main_sysfs.sh onto the board. 

Run these files on the galileo serial termial by running the following command
		sudo screen /dev/ttyUSB0 115200

inside the serial screen ternimal enter
   $insmod platform_device.ko
   $insmod platform_driver.ko	(the steps 1 and 2 can be interchanged)


After the platform device and the driver are matched, the driver is probed to intialise and register the device. Since there are
two sensors, the driver probe function is called twice. simultaneously the class HCSR, the device subdirectories HCSRdevice1 and
HCSRdevice2 and their respective attributes are created.

Make teh shell script executabel -

chmod +x Main_sysfs.sh

And then run the shell scrip - 

./Main_sysfs.sh

It will ask for echo and trigger pin for two devices and it will trigger the measurement for the end . At last it will read the distance
