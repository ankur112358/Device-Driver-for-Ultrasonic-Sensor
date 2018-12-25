#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/errno.h>
#include "platform_device.h"
#include "muxtable.h"

int errno;
int count =0;

static struct class *gko_class;
static dev_t gko_dev= 1;

static int first = 1;

/* defining table for platform devices
for matching purpose
*/

static const struct platform_device_id Plat_id_table[] = {
         { DEVICE_NAME1, 0 },
         { DEVICE_NAME2, 0 },
};

static int No_of_devices = 0;
static LIST_HEAD(device_list);

/////function get current timestamp counter

unsigned long long get_tsc(void){
   unsigned a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}

struct buff *all_samples;
unsigned long long  tsc_raising, tsc_falling;
struct timespec time_raising, time_falling;
static struct hcsr_dev* HEAD;

////interrupt handler function

static irq_handler_t interrupt_handler(unsigned int irq, void *dev_id) {

	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)dev_id;
	int distance;
	long difference;
	struct 	data measurement;
	int check = gpio_get_value(hcsr_obj->pin.gpio_echo_pin);
	 printk(KERN_INFO"INTERRUPT HANDLER\n");

	if (irq == gpio_to_irq(hcsr_obj->pin.gpio_echo_pin))
	{
		if (check ==0)
		{
            printk("gpio pin is low\n");
	 		tsc_falling=get_tsc();
			getnstimeofday(&time_falling);      /////after getting the timestamp for rising edge, this will get the time stamp for failling edge
			difference =  (time_falling.tv_nsec - time_raising.tv_nsec)/1000;//////calculating the difference between 2 timestamp to find the pulse width
			distance = difference / 58;                                      //////converting pulse width into distance in cm
			measurement.timestamp = tsc_falling;
			measurement.distance = distance;
			printk("Current Distance %d\n", distance);
			insert_buffer(&hcsr_obj->all_samples, measurement, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock);
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);                /////reseting back the irq type
  		}
		else
		{
			printk("gpio pin is high\n");
			tsc_raising = get_tsc();                            /////get get raising edge timestamp first
			getnstimeofday(&time_raising);
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);          /////modify the irq type to call the handler for falling edge

		}
	}

   return (irq_handler_t) IRQ_HANDLED;
}

/*this function is used to get the interrupt number
from gpio and setting up the interrup handler
*/

int set_interrupt(void *device_struct){

    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;
    int irqNumber, result;
    irqNumber = gpio_to_irq(hcsr_obj->pin.gpio_echo_pin);
	if(irqNumber < 0){
		printk("GPIO to IRQ mapping failure \n" );
      		return -1;
	}
    printk(KERN_INFO "IRQNUMBER is %d for pin %d\n", irqNumber, hcsr_obj->pin.gpio_echo_pin);

    result = request_irq(irqNumber,               	// The interrupt number requested
		 (irq_handler_t) interrupt_handler, 	    // The pointer to the handler function (above)
		 IRQF_TRIGGER_RISING,                 		// Interrupt is on rising edge
		 "ebb_gpio_handler",                  		// Used in /proc/interrupts to identify the owner
		 (void*)hcsr_obj);                     // The *dev_id for shared interrupt lines, NULL here

    if(result < 0){
         printk("Irq Request failure\n");
		return -1;
    }
    return 0;
}

/////this function is used to free the interrupt once job is done

int release_irq(void* handle)
{
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)handle;
	printk(KERN_INFO "Releasing the interrupt %d on pin %d\n", gpio_to_irq(hcsr_obj->pin.gpio_echo_pin), hcsr_obj->pin.gpio_echo_pin);
	free_irq(gpio_to_irq(hcsr_obj->pin.gpio_echo_pin), handle );
	return 0;
}

/*depending upon the pin number and its function (input/output)
provided from set_mux(), this function will fetch its corresponding
 mux and level shifter gpio pins and set them up*/

int set_pin_conf(int pin, bool func){

	struct conf *config = NULL;
	config = (struct conf*)get_pin(pin,func);
	if(config == NULL){
		return -EINVAL;
	}

	if(func == INPUT_FUNC){
        	printk(KERN_INFO "Setting gpio %d as input\n", config->gpio_pin.pin);
       		gpio_request(config->gpio_pin.pin, "Input");
		gpio_direction_input(config->gpio_pin.pin);
	}else{
		printk(KERN_INFO "Setting gpio %d as trigger\n", config->gpio_pin.pin);
       		gpio_request(config->gpio_pin.pin, "trigger");
		gpio_direction_output(config->gpio_pin.pin, 0);
	}
	if(config->direction_pin.pin != DONT_CARE){
        printk(KERN_INFO "Setting direction gpio  %d\n", config->direction_pin.pin);
		gpio_request(config->direction_pin.pin, "Direction");
		gpio_direction_output(config->direction_pin.pin, config->direction_pin.level);
	}
	if(config->function_pin_1.pin != DONT_CARE){
        printk(KERN_INFO "Setting function 1 gpio  %d\n", config->function_pin_1.pin);
        	gpio_request(config->function_pin_1.pin, "function_1");
		if (gpio_direction_output(config->function_pin_1.pin, config->function_pin_1.level) == -EIO){
			gpio_set_value(config->function_pin_1.pin, config->function_pin_1.level);
		}
	}
	if(config->function_pin_2.pin != DONT_CARE){
        	printk(KERN_INFO "Setting function 1 gpio  %d\n", config->function_pin_2.pin);
        	gpio_request(config->function_pin_2.pin, "function_2");
		gpio_direction_output(config->function_pin_2.pin, config->function_pin_2.level);
	}
	return 	config->gpio_pin.pin;

}
/*once the job is done this
function will free the gpio pins
*/


int mux_free_pins(int pin, bool func){

	struct conf *config = NULL;
	config = (struct conf*)get_used_pins(pin,func);
	if(config == NULL){
		return -EINVAL;
	}

	printk(KERN_INFO "Freeing gpio  %d\n", config->gpio_pin.pin);
	gpio_set_value_cansleep(config->gpio_pin.pin, 0);
	gpio_free(config->gpio_pin.pin);

	if(config->direction_pin.pin != DONT_CARE){
        	printk(KERN_INFO "Freeing direction gpio  %d\n", config->direction_pin.pin);
       		gpio_set_value_cansleep(config->direction_pin.pin, 0);
		gpio_free(config->direction_pin.pin);
	}
	if(config->function_pin_1.pin != DONT_CARE){
       		printk(KERN_INFO "Freeing function 1 gpio  %d\n", config->function_pin_1.pin);
        	gpio_set_value_cansleep(config->function_pin_1.pin, 0);
		gpio_free(config->function_pin_1.pin);
	}
	if(config->function_pin_2.pin != DONT_CARE){
        	printk(KERN_INFO "Freeing function 1 gpio  %d\n", config->function_pin_2.pin);
        	gpio_set_value_cansleep(config->function_pin_2.pin, 0);
		gpio_free(config->function_pin_2.pin);
	}

	return 1;

}

///// this fetches the trigger and echo pins per device

int set_mux(struct pins* pins, void* device_struct){

    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;


    int gpio_trigger_pin, gpio_echo_pin;
    gpio_echo_pin = set_pin_conf(pins->echo_pin,INPUT_FUNC);
    gpio_trigger_pin= set_pin_conf(pins->trigger_pin,OUTPUT_FUNC);

    hcsr_obj->pin.trigger_pin = pins->trigger_pin ;
    hcsr_obj->pin.echo_pin = pins->echo_pin;
    hcsr_obj->pin.gpio_trigger_pin = gpio_trigger_pin ;
    hcsr_obj->pin.gpio_echo_pin = gpio_echo_pin;

    printk(KERN_INFO "Echo Pin  %d and Trigger Pin %d\n",hcsr_obj->pin.echo_pin, hcsr_obj->pin.trigger_pin);
    printk(KERN_INFO "Echo gpio  %d and Trigger gpio %d\n",hcsr_obj->pin.gpio_echo_pin, hcsr_obj->pin.gpio_trigger_pin);

    return 1;

}

/*when called upon this function will start
sampling by sending the trigger pulse, stores
the distance and its timestamp in fifo buffer
*/


static int start_sampling(void *handle)
{
     struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)handle;
    int counter = 0;
    int i,total_distance, distance = 0;
    struct data measurement;
    unsigned long long timestamp;

	if(set_interrupt((void*)hcsr_obj) < 0){
	     printk(KERN_INFO "Error While setting up the interrupt\n");
	     return -1;
    	}

    	clear_buffer(&hcsr_obj->all_samples, hcsr_obj->param.number_of_samples+2, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock);

	printk("entered thread with echo pin %d and trigger pin %d  %d %d\n", hcsr_obj->pin.gpio_echo_pin, hcsr_obj->pin.gpio_trigger_pin, hcsr_obj->param.delta, hcsr_obj->param.number_of_samples);
	do{
        printk("Sample *** %d \n", counter);
		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 0);
		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 1);
		udelay(30);
   		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 0);          /////Sending Low-High-Low for sending the trigger
		mdelay(hcsr_obj->param.delta);
		counter++;
	}while(!kthread_should_stop() && counter < hcsr_obj->param.number_of_samples + 2);

    total_distance = 0;
    timestamp = get_tsc();
    for(i = 1;i < hcsr_obj->param.number_of_samples+1; i++){
        distance += read_fifo(&hcsr_obj->all_samples, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock).distance;
    }

    measurement.timestamp = timestamp;
    measurement.distance = distance/hcsr_obj->param.number_of_samples;
    insert_buffer(&hcsr_obj->buffer, measurement, &hcsr_obj->buffer.sem, &hcsr_obj->buffer.Lock);           //// inserting the timestamp and distance in fifo buffer
    printk("Calculating distance %d \n", distance/hcsr_obj->param.number_of_samples);
    release_irq((void*)hcsr_obj);
    hcsr_obj->is_in_progress = false;

    return 0;
}


int trigger(struct hcsr_dev *device_data, int input) {

	if(device_data->is_in_progress){
		printk(KERN_INFO "On-Going Measurement\n");                 /////error handling to avoid concurrency issue while ongoing measurement
		return -EINVAL;
	}else{
		if(input  > 0) {
                        printk(KERN_INFO "Clearing the buffer\n");
			clear_buffer(&device_data->buffer, MAX_BUFF_SIZE, &device_data->buffer.sem, &device_data->buffer.Lock);
		}

		printk(KERN_INFO "Starting the Sampling now!!!!!\n");
		device_data->is_in_progress = true;
		kthread_run(&start_sampling,(void *)device_data, "start_sampling");         //////kernel thread initiating the start sampling function

	}

	return 0;

}

///// char device file ops wirte function

ssize_t start_measurement(struct file *file, const char *buff, size_t size, loff_t *lt){

	struct hcsr_dev *device_data = file->private_data;
	int *input;
	// printk(KERN_INFO "%s starting the sampling\n", device_data->device.name);
	if (!(input= kmalloc(sizeof(int), GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}

	if (copy_from_user(input, buff, sizeof(int))){
		return -EFAULT;
	}

	if(trigger(device_data, *input) < 0){
		return -1;
	}

	kfree(input);

	return 1;
}

//// char device read function to read value for buffer

ssize_t read_buffer(struct file *file, char *buff, size_t size, loff_t *lt) {
	struct hcsr_dev *device_data = file->private_data;
	if(device_data->buffer.count > 0 || device_data->is_in_progress){
		struct data readings = read_fifo(&device_data->buffer, &device_data->buffer.sem, &device_data->buffer.Lock);
		copy_to_user(buff, &readings, sizeof(struct data));
	}else{
	    start_measurement(file, buff, size, lt);
	}
	return size;
}

/*show function for displaying
the echo pin in sysfs
*/

static ssize_t show_echo_pin(struct device *dev_struct, struct device_attribute *attr, char *buf)
{
	struct hcsr_dev *device_struct  = dev_get_drvdata(dev_struct);
        return snprintf(buf, PAGE_SIZE, "%d\n", device_struct->pin.echo_pin);
}

/* show function for displaying
the distance in sysfs
*/

static ssize_t show_distance(struct device *dev_struct, struct device_attribute *attr, char *buf)
{

       struct hcsr_dev *device_struct  = dev_get_drvdata(dev_struct);
	struct data readings = device_struct->buffer.result[0];
	return snprintf(buf, PAGE_SIZE, "%d\n",readings.distance);

}

/* show function for enable
*/

static ssize_t show_enable(struct device *dev_struct, struct device_attribute *attr, char *buf)
{
        struct hcsr_dev *device_struct  = dev_get_drvdata(dev_struct);
        return snprintf(buf, PAGE_SIZE, "%d\n", device_struct->enable);
}

/* show function for displaying the
trigger pin */


static ssize_t show_trigger_pin(struct device *dev_struct, struct device_attribute *attr, char *buf)
{

	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);
        return snprintf(buf, PAGE_SIZE, "%d\n", device_struct->pin.trigger_pin);
}

/* show function for displaying
the number of samples
*/

static ssize_t show_sample_numbers(struct device *dev_struct, struct device_attribute *attr, char *buf)
{

	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);
        return snprintf(buf, PAGE_SIZE, "%d\n", device_struct->param.number_of_samples);
}

/* show function for displaying
the sampling perios
*/

static ssize_t show_sampling_period(struct device *dev_struct, struct device_attribute *attr, char *buf)
{

	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);
        return snprintf(buf, PAGE_SIZE, "%d\n", device_struct->param.delta);
}


/*Store functions for setting
up the trigger pin enterer in sysfs
*/
static ssize_t store_trigger_pin(struct device *dev_struct,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
	int gpio_trigger_pin;
	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);

        sscanf(buf, "%d", &device_struct->pin.trigger_pin);

   	device_struct->pin.gpio_trigger_pin = gpio_trigger_pin = set_pin_conf(device_struct->pin.trigger_pin,OUTPUT_FUNC);

   	printk(KERN_INFO "Trigger Pin %d\n",device_struct->pin.trigger_pin);
    	printk(KERN_INFO "Trigger gpio %d\n",device_struct->pin.gpio_trigger_pin);

	return count;

}

/* store function for setting
up the echo pin in sysfs
*/

static ssize_t store_echo_pin(struct device *dev_struct,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);

        sscanf(buf, "%d", &device_struct->pin.echo_pin);
	device_struct->pin.gpio_echo_pin = set_pin_conf(device_struct->pin.echo_pin,INPUT_FUNC);

    	printk(KERN_INFO "echo Pin %d\n",device_struct->pin.echo_pin);
    	printk(KERN_INFO "echo gpio %d\n",device_struct->pin.gpio_echo_pin);

	return count;

}

/* store function for setting up the
number of samples entered in sysfs
*/

static ssize_t store_sample_numbers(struct device *dev_struct,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);

        sscanf(buf, "%d", &device_struct->param.number_of_samples);
	 if(alloc_buffer(&device_struct->all_samples, device_struct->param.number_of_samples + 2) < 0){
		printk(KERN_DEBUG "ERROR while allocating memory\n");
		return -1;
	    }
	init_buffer(&device_struct->all_samples, MAX_BUFF_SIZE, &device_struct->all_samples.sem,  &device_struct->all_samples.Lock );
	return count;

}

/* store function for setting up the
sampling perios in sysfs
*/

static ssize_t store_sampling_period(struct device *dev_struct,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
	struct hcsr_dev *device_struct = dev_get_drvdata(dev_struct);

        sscanf(buf, "%d", &device_struct->param.delta);

	return count;

}

/* store function for enable directory
*/

static ssize_t enable(struct device *dev_struct,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
	int input;
	struct hcsr_dev *device_data = dev_get_drvdata(dev_struct);
	sscanf(buf, "%d",&input);

	if(input == 1){
		if (trigger(device_data, 0) < 0){
			return -EINVAL;
		}
	}else if(input == 0){
		 if(device_data->is_in_progress)
		{
			device_data->is_in_progress = false;
			printk("Stopping the current thread\n");
		}
	}

	printk("write freed\n");
	return count;
}



static DEVICE_ATTR(sampling_period,S_IRWXU, show_sampling_period, store_sampling_period);
static DEVICE_ATTR(sample_numbers,S_IRWXU, show_sample_numbers, store_sample_numbers);
static DEVICE_ATTR(trigger_pin,S_IRWXU, show_trigger_pin, store_trigger_pin);
static DEVICE_ATTR(echo_pin,S_IRWXU, show_echo_pin, store_echo_pin);
static DEVICE_ATTR(enable,S_IRWXU, show_enable, enable);
static DEVICE_ATTR(distance, S_IRUSR, show_distance, NULL);


int device_open(struct inode *inode, struct file *file){
	int dev_minor= iminor(inode);
	struct hcsr_dev* temp = HEAD;

	while(temp != NULL){
	//printk(KERN_INFO "checking device %d\n", temp->device.minor);
		if(temp->device.minor == dev_minor){
		  file->private_data = temp;
		  printk(KERN_INFO "%s device is opened\n", temp->device.name);
		  break;
		}
		temp = temp->next;
	}
	if(temp == NULL){
		printk(KERN_INFO "Device not found with minor number %d\n", dev_minor);
		return -1;
	}
	return 0;
}


bool validate_pins(struct pins *pin){
	if(pin->echo_pin < 0 || pin->trigger_pin < 0){
		return false;
	}

	if(!check_pin(pin->echo_pin,INPUT_FUNC)){
		return false;
	}

	if(!check_pin(pin->trigger_pin,OUTPUT_FUNC)){
		return false;
	}

	return true;
}
/**

	IOCTL for Seeting pins and configuration of HCSR device

*/
long ioctl_handle(struct file *file, unsigned int command, unsigned long ioctl_param){
	struct pins *pin_conf;
	struct parameters* param;
	struct hcsr_dev *device_data = file->private_data;
	printk(KERN_INFO "INSIDE IOCTL\n");
	switch(command){
		case CONFIG_PINS:
                    printk(KERN_INFO "Setting the Pins\n");
					pin_conf = kmalloc(sizeof(struct pins), GFP_KERNEL);
					/*if(!validate_pins(pin_conf)){
						return -EINVAL;
					}*/
					copy_from_user(pin_conf, (struct pins*) ioctl_param, sizeof(struct pins));
                   			 set_mux(pin_conf,(void*)device_data);
					kfree(pin_conf);
					break;
		case SET_PARAMETERS:
                    printk(KERN_INFO "Setting the parameters for the %s\n", device_data->device.name);
					param = kmalloc(sizeof(struct parameters), GFP_KERNEL);
					copy_from_user(param, (struct parameters*) ioctl_param, sizeof(param));
					device_data->param.number_of_samples = param->number_of_samples;
					device_data->param.delta = param->delta;
                    if(alloc_buffer(all_samples, device_data->param.number_of_samples + 2) < 0){
                        printk(KERN_DEBUG "ERROR while allocating memory");
                        return -1;
                    }
					kfree(param);
					break;
		default: return -EINVAL;
	}


	return 1;
}

int release(struct inode *inode, struct file *file){
	// release_irq((void*)device_struct);
	printk(KERN_INFO "closing device \n");
	return 0;
}

static struct file_operations fops = {

	.open = device_open,
	.write = start_measurement,
	.read = read_buffer,
	.unlocked_ioctl = ioctl_handle,
	.release = release,
};
////init function , used for registering misc devices

struct hcsr_dev* init_device(void * plat_chip){

    int  error;
    struct hcsr_dev* device_struct;
    static struct hcsr_dev* temp = NULL;
    struct HCSRChipdevice * plat_device = (struct HCSRChipdevice *)plat_chip;

    	printk(KERN_INFO "Registering HCSR device");
	device_struct = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);

	if(!device_struct) {
	   printk(KERN_DEBUG "ERROR while allocating memory");
	   return NULL;
	}

    	memset(device_struct, 0, sizeof (struct hcsr_dev));
	device_struct->device.minor = MISC_DYNAMIC_MINOR;
	device_struct->device.name = plat_device->name;
	device_struct->dev_name = plat_device->name;
	device_struct->device.fops = &fops;

	error = misc_register(&device_struct->device);

	if(error){
		printk("Error while registering device");
		return NULL;
	}

	all_samples = kmalloc(sizeof(struct buff), GFP_KERNEL);
	    if(alloc_buffer(&device_struct->buffer, MAX_BUFF_SIZE) < 0){
		printk(KERN_DEBUG "ERROR while allocating memory");
		   return NULL;
		}

	device_struct->next = NULL;

	if(HEAD == NULL){
		HEAD = device_struct;
		temp = HEAD;
	}else{
		device_struct->next = HEAD;
		HEAD = device_struct;
	}

	init_buffer(&device_struct->buffer,MAX_BUFF_SIZE, &device_struct->buffer.sem,  &device_struct->buffer.Lock);
	printk(KERN_INFO "Driver %s is initialised\n", CLASS_NAME);

	return device_struct;

}

/*platform probe function.
Gets called when a corresponding device
is founc
*/

static int Plat_driver_probe(struct platform_device *device_found)
{

	int rval;
	struct hcsr_dev* device_struct;
	struct HCSRChipdevice * plat_device;

	plat_device = container_of(device_found, struct HCSRChipdevice , dev);

	printk(KERN_ALERT "Found the device -- %s  %d \n", plat_device->name, plat_device->dev_n);
	if(first == 1)
	{
		gko_class = class_create(THIS_MODULE, CLASS_NAME);              //creation of class for sysfs
			if (IS_ERR(gko_class)) {
				printk( " cant create class %s\n", CLASS_NAME);
				goto class_err;

			}
		first =0;
	}
	device_struct = init_device((void*)plat_device);

	if(device_struct == NULL)
	{
		printk("device initialisation failed\n");
		return -1;
	}

	INIT_LIST_HEAD(&device_struct->device_entry) ;
	list_add(&device_struct->device_entry, &device_list );          //linked list for handling multiple devices

	/* device */
        plat_device->gko_device = device_create(gko_class, NULL, gko_dev, device_struct, device_struct->dev_name);          //creating a device under class in sysfs
        if (IS_ERR(plat_device->gko_device)) {

                printk( " cant create device %s\n", device_struct->dev_name);
                goto device_err;
        }

	printk("device create\n");

	/* creation of directory like trigger, echo, enable
	under device
	*/

	rval = device_create_file(plat_device->gko_device, &dev_attr_trigger_pin);
        if (rval < 0) {
                printk(" cant create device attribute %s %s\n",
                       device_struct->dev_name, dev_attr_trigger_pin.attr.name);
        }

	rval = device_create_file(plat_device->gko_device, &dev_attr_echo_pin);
        if (rval < 0) {
               printk(" cant create device attribute %s %s\n",
                       device_struct->dev_name, dev_attr_echo_pin.attr.name);
        }


	rval = device_create_file(plat_device->gko_device, &dev_attr_enable);
        if (rval < 0) {
     		printk(" cant create device attribute %s %s\n",
                device_struct->dev_name, dev_attr_enable.attr.name);
        }

	rval = device_create_file(plat_device->gko_device, &dev_attr_distance);
        if (rval < 0) {
   	 printk(" cant create device attribute %s %s\n",
                       device_struct->dev_name, dev_attr_distance.attr.name);
        }

	rval = device_create_file(plat_device->gko_device, &dev_attr_sampling_period);
        if (rval < 0) {
   	 printk(" cant create device attribute %s %s\n",
                       device_struct->dev_name, dev_attr_sampling_period.attr.name);
        }


	rval = device_create_file(plat_device->gko_device, &dev_attr_sample_numbers);
        if (rval < 0) {
   	 printk(" cant create device attribute %s %s\n",
                       device_struct->dev_name, dev_attr_sample_numbers.attr.name);
        }

	printk("probe done\n");
	No_of_devices++;

	return 0;
	/* error handling in is case of
	error due to class or device creation
	*/

	device_err:
       		 device_destroy(gko_class, gko_dev);
	class_err:
        	class_unregister(gko_class);
        	class_destroy(gko_class);
	return -EFAULT;

};

static int Plat_driver_remove(struct platform_device *plat_dev)
{
	struct hcsr_dev* device_struct;
	struct HCSRChipdevice * plat_device;

	printk("enter remove of the probe\n");
	plat_device = container_of(plat_dev, struct HCSRChipdevice , dev);

	/* deleting files, device
	on unregistering
	*/

	list_for_each_entry(device_struct, &device_list, device_entry)
	{
	    	printk("remove a list %s ---- %s \n",device_struct->dev_name,plat_dev->name );
		if(device_struct->dev_name == plat_dev->name)
		{

		    list_del(&device_struct->device_entry);
		    device_remove_file(plat_device->gko_device, &dev_attr_trigger_pin);
		    device_remove_file(plat_device->gko_device, &dev_attr_echo_pin);
		    device_remove_file(plat_device->gko_device, &dev_attr_enable);
                    device_remove_file(plat_device->gko_device, &dev_attr_distance);
		    device_remove_file(plat_device->gko_device, &dev_attr_sampling_period);
		    device_remove_file(plat_device->gko_device, &dev_attr_sample_numbers);
		    device_destroy(gko_class, plat_device->gko_device->devt);
	    	    device_destroy(gko_class, device_struct->device.minor);
		    misc_deregister(&device_struct->device);
	   	    printk("freed misc\n");
		    kfree(device_struct);
	   	    printk("freed obj\n");
		    No_of_devices--;
		    break;

		}
	}

	/* if(No_of_devices == 0){
		printk("Destroying the class\n");
		class_unregister(gko_class);
        	class_destroy(gko_class);
	}*/

	return 0;

}

static struct platform_driver Plat_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= Plat_driver_probe,
	.remove		= Plat_driver_remove,
	.id_table	= Plat_id_table,
};

module_platform_driver(Plat_driver);
MODULE_LICENSE("GPL");
