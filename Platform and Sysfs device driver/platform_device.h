#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
//#include <sys/ioctl.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>    // kernel stuff
#include <linux/gpio.h>      // GPIO functions/macros
#include <linux/interrupt.h> // interrupt functions/macros
#include <linux/time.h>
#include <linux/unistd.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "buffer.h"
#include <linux/ioctl.h>
#define MAX_BUFF_SIZE 1



#define GPIO_INT_NAME "gpio_int"


#define CLASS_NAME "HCSR"
#define DRIVER_NAME "HCSR_DRV"
#define DEVICE_NAME1 "HCSRdev1"
#define DEVICE_NAME2 "HCSRdev2"


#ifndef __SAMPLE_PLATFORM_H__

#define __SAMPLE_PLATFORM_H__

#define IOCTL_APP_TYPE 80
#define CONFIG_PINS _IOWR(IOCTL_APP_TYPE, 1, struct pins)     // ioctl to config pins of HCSR

#define SET_PARAMETERS _IOWR(IOCTL_APP_TYPE, 2, struct parameters)     // ioctl to config pins of HCSR

/* defining the structure of
platform driver
*/

struct HCSRChipdevice {

		char 			*name;
		int			dev_n;
		struct device *gko_device;
		struct platform_device 	dev;

};



struct pins {
	int echo_pin;
	int trigger_pin;            ////pins as per Galileo board
	int gpio_echo_pin;
	int gpio_trigger_pin;           ////corresponding gpio numbers of the pins
};

struct parameters {
	int number_of_samples;
	int delta;
};

struct hcsr_dev {
   	struct HCSRChipdevice* plat_device;
    	struct miscdevice device;
	struct pins pin;
	struct parameters param;
	struct buff buffer;
	struct list_head device_entry;
	struct task_struct *task;
	char *dev_name;
	bool is_in_progress;
	struct buff all_samples;
	struct hcsr_dev *next;
	int enable;
};


#endif /* __GPIO_FUNC_H__ */

