#!/bin/bash

#Setting trigger and echo pins for device 1
	echo -n "Enter trigger pin for device 1 -> "
	read trigger_pin
	echo "$trigger_pin" > /sys/class/HCSR/HCSRdev1/trigger_pin
	echo -n "Enter echo pin  for device 1 -> "
	read echo_pin
	echo "$echo_pin" > /sys/class/HCSR/HCSRdev1/echo_pin
	echo -n "Enter Number of samples  for device 1 -> "
	read sample_number
	echo "$sample_number" > /sys/class/HCSR/HCSRdev1/sample_numbers
	echo -n "Enter sampling period for device 1 -> "
	read sampling_period
	echo "$sampling_period" > /sys/class/HCSR/HCSRdev1/sampling_period
	
#Setting trigger and echo pins for device 2
	echo -n "Enter trigger pin for device 2 -> "
	read trigger_pin
	echo "$trigger_pin" > /sys/class/HCSR/HCSRdev2/trigger_pin
	echo -n "Enter echo pin  for device 2 -> "
	read echo_pin
	echo "$echo_pin" > /sys/class/HCSR/HCSRdev2/echo_pin
	echo -n "Enter Number of samples  for device 2 -> "
	read sample_number
	echo "$sample_number" > /sys/class/HCSR/HCSRdev2/sample_numbers
	echo -n "Enter sampling period for device 2 -> "
	read sampling_period
	echo "$sampling_period" > /sys/class/HCSR/HCSRdev2/sampling_period
	
#Case 1(device 1): Start measurement
	echo "Case 1(device 1): Start Measurement(enable=1)"
	echo "1" > /sys/class/HCSR/HCSRdev1/enable
	
	sleep 10
	
#Reading distance value device 1
	echo "distance measurement device 1"
	cat /sys/class/HCSR/HCSRdev1/distance
	

#Case 2(device 2): Start measurement
	echo "Case 1(device 2): Start Measurement(enable=1)"
	echo "1" > /sys/class/HCSR/HCSRdev2/enable
	
	sleep 10
#Reading distance value device 2
	echo "distance measurement device 2"
	cat /sys/class/HCSR/HCSRdev2/distance

