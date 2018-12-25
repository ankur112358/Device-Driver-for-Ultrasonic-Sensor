#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "hcsr.h"
#include "data.h"

/* read measurement function to
randomly call read file
*/

void read_measurement(int fd, int devie_number){

     //int fd = (int*)arg;
     int res;
     struct data measurement;

	if((res = read(fd, &measurement, sizeof(struct data))) < 0) {
		//printf("Error in reading the measurement from device %d \n", devie_number);
     	}else
	{
		 printf("Current distance measurement for device %d - Time :- %llu  Distance :- %dcm\n", devie_number,measurement.timestamp,  measurement.distance);

	}
	sleep(4);

	if((res = read(fd, &measurement, sizeof(struct data))) < 0) {
		//printf("Error in reading the measurement from device %d \n", devie_number);
     	}else
	{
   		 printf("Current distance measurement for device %d - Time :- %llu  Distance :- %dcm\n", devie_number,measurement.timestamp,  measurement.distance);
	}


}

/* write measurement function
to randomly call write on file
*/

void start_measurement(int fd, int devie_number){

     //int fd = (int*)arg;
     int res;
     int enable = 0;

     if((res = write(fd, &enable, sizeof(int))) < 0) {
	printf("Error in starting the measurement\n");
     }
     sleep(4);
     if((res = write(fd, &enable, sizeof(int)))< 0) {
	printf("Error in starting the measurement\n");
     }
     if((res = write(fd, &enable, sizeof(int)))< 0) {
		printf("Error in starting the measurement\n");
     }
     if((res = write(fd, &enable, sizeof(int)))< 0) {
	printf("Error in starting the measurement\n");
     }

}


int main(int argc, char **argv)
{
int i,number_of_devices;
    struct pins *pin_conf;
    struct parameters *params;
    int* fd;
    char *name;

    printf("Please enter the number of devices:-");
    scanf("%d", &number_of_devices);
    fd = malloc(sizeof(int)*number_of_devices);
    pin_conf= malloc(sizeof(struct pins)*number_of_devices);
    params = malloc(sizeof(struct parameters)*number_of_devices);

	if(argc > 1 && strcmp("ioctl", argv[1]) == 0){                      ////fetching pin numbers for echo and trigger pins per device from the user
	    for(i=0;i<number_of_devices;i++){
			printf("Please enter the echo pin for device %d:-", i);
		   	scanf("%d", &pin_conf[i].echo_pin);
			printf("Please enter the trigger pin for device %d:-", i);
		   	scanf("%d", &pin_conf[i].trigger_pin);
			printf("Please enter the number of samples for device %d:-", i);                /////fecthing number of samples and sampling delay per device from user
		   	scanf("%d", &params[i].number_of_samples);
			printf("Please enter the delay for device %d:-", i);
		   	scanf("%d", &params[i].delta);
	    }
	}

    for(i=0;i<number_of_devices;i++){
		if (!(name = malloc(sizeof(char)*40)))
		{
			printf("Bad Kmalloc\n");
			return -ENOMEM;
		}
		 memset(name, 0, sizeof(char)*40);
		snprintf(name, sizeof(char)*40, "/dev/%s_%d", DEVICE_NAME_PREFIX, i);
		fd[i] = open(name, O_RDWR);

    }

	if(argc > 1 && strcmp("ioctl", argv[1]) == 0){

	    for(i=0;i<number_of_devices;i++){                                       /////calling the ioctl function per device
			if(ioctl(fd[i], SET_PARAMETERS, (unsigned long)&params[i]) < 0){
			printf("Error while setting parameters \n");
				return -1;
			}
			if(ioctl(fd[i], CONFIG_PINS, (unsigned long)&pin_conf[i]) < 0){
				printf("Error while setting pins\n");
				return -1;
			}
	    }

	}

	/* calling the read and write
	mesurement function per device
	*/

	for(i=0;i<number_of_devices;i++){

		read_measurement(fd[i],i);
	}

	sleep(2);

	for(i=0;i<number_of_devices;i++)
	{
		start_measurement(fd[i],i);
	}
	sleep(2);

	for(i=0;i<number_of_devices;i++){

		read_measurement(fd[i],i);
	}

	return 0;
}
