# StrideFly-V2-GPSTracker
Code for StrideFly RadioFrequency GPS Tracker (TI Tiva C Launchpad)

StrideFly is a live tracking system for outdoor running events. Prototype V2 implemented a closed system (i.e. no web or other connectivity 
required) that used 900Mhz XBee radios to transmit GPS positions of runners to the tracking client (a windows tablet) at the base camp.

The code in this repository is for the Texas Instruments Tiva C Launchpad (an ARM-based development platform) that is the base for the 
GPS tracker prototype. The program controls the basic functioning of the microprocessor (looping, interrupts, etc.), polling the GPS module
for updated coordinates and transmitting the positions using the connected 900Mhz XBee radio.
