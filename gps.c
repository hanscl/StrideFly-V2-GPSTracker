//*****************************************************************************
//
// gps.c - Tiva C Launchpad GPS Module for StrideFly Tracker
//
// Copyright (c) 2014 StrideFly, LLC.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************
#include <stdint.h>
#include <stdlib.h> // Need this and ustdlib?
#include "utils/ustdlib.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
// Tiva C includes
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
// Project includes
#include "gps.h"
#include "xbee.h"
//#include "stridefly.h"

//*****************************************************************************
//
// Global variables for state of GPS
//
//*****************************************************************************
tGpsState g_sGpsState;

//*****************************************************************************
//
// Function Prototypes only used in gps.c
//
//*****************************************************************************
uint32_t NMEAChecksum(char *nmeaMsg);
void SendToGPS(char *nmeaMsg);


//*****************************************************************************
//
// Query GPS
//
//*****************************************************************************
void QueryGPS(uint8_t ui8MsgSelect)
{
	// Declare Variables
	uint32_t bufSize = 32;
	uint32_t nmeaChecksum;

	char psrfCore[] = "PSRF103,%02u,%02u,%02u,01";
	char psrfWrap[] = "$%s*%x\r\n";

	// Turn on LED while we process button push
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02);

    // Allocate space for buffers
    char* psrfBuffer = malloc(bufSize);
    char* psrfFinal = malloc(bufSize);

    // format the core string
    sprintf(psrfBuffer, psrfCore, ui8MsgSelect, 1, 0);

    // get checksum for nmea message
    nmeaChecksum = NMEAChecksum(psrfBuffer);

    // format the complete NMEA command string
    sprintf(psrfFinal, psrfWrap, psrfBuffer, nmeaChecksum);

    // Debug output to console
    puts(psrfFinal);

    SendToGPS(psrfFinal);

    // Clean up heap
    free(psrfBuffer);
    free(psrfFinal);

    // turn led back off
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);
}

//*****************************************************************************
//
// Process NMEA received from GPS
//
//*****************************************************************************
void ProcessReceivedNMEA(char *nmeaSentence) {

	const char gpsMsg[] = "$GPRMC";
	const char cfgMsg[] = "$PSRF";

	// Strings for determining fix
	const char nmeaSep[] = ",";
	char nmeaInvalid = 'V';

	const char gpsReady[] = "$PSRF150,1";
	const char gpsTrickle[] = "$PSRF150,0";

	char *gpsFix;

	puts("Received complete NMEA Sentence");
	printf("String Length: %u", strlen(nmeaSentence));
	puts(nmeaSentence);


	if(strncmp(nmeaSentence, cfgMsg, strlen(cfgMsg)) == 0) {
		puts("configuration message");
		// If the GPS is ready to receive commands; send the GPRMC query
		if(strncmp(nmeaSentence, gpsReady, strlen(gpsReady)) == 0)
		{
			g_sGpsState.i8OnOff = 1; // it's on now
			puts("GPS is ON");
			QueryGPS(NMEA_MSG_RMC);
		}
		// if we receive the hibernate message, just save to global GPS state
		else if(strncmp(nmeaSentence, gpsTrickle, strlen(gpsTrickle)) == 0) {
			g_sGpsState.i8OnOff = 0; // it's off now
			puts("GPS is OFF");
		}
	// Here we ahve received a valid GPRMC message. Go ahead and send it out via XBee Module
	} else if(strncmp(nmeaSentence, gpsMsg, strlen(gpsMsg)) == 0) {
		puts("gps message");

		TransmitViaXBee(nmeaSentence);

		// Check if the GPS module has a valid fix.  If it does, we can hibernate because hot start is only 1 second
		// If it doesn't have a fix yet, we will keep it on to keep searching fix.
		// Careful, this means that if the tracker is on indoors it will drain the battery!!!
		gpsFix = strpbrk(nmeaSentence, nmeaSep); // First section
		gpsFix = strpbrk(gpsFix+1, nmeaSep); // Second section -> this is either A (valid) or V (invalid = no fix)

		if(gpsFix[1] == nmeaInvalid) {
			puts("GPS Module has no fix yet. Do not turn off");
		}
		// We're done sending. Turn off the GPS (make sure it is actually on)
		else if(g_sGpsState.i8OnOff == 1) {
			ToggleGPS();
		}
	}

	puts("Processing Complete");
}


//*****************************************************************************
//
// Send NMEA Message to GPS
//
//*****************************************************************************
void SendToGPS(char* nmeaMsg) {
	uint32_t charCnt, length;

	length = strlen(nmeaMsg);

	for(charCnt = 0; charCnt < length; charCnt++) {
		UARTCharPut(UART1_BASE,nmeaMsg[charCnt]);
	}
}

//*****************************************************************************
//
// Calculate NMEA Checksum
//
//*****************************************************************************
uint32_t NMEAChecksum(char *nmeaMsg) {

	uint32_t length, checksum, charCnt;

    // Initialize variables for checksum calculation
    length = ustrlen(nmeaMsg);
    checksum = 0;

    // Calculate the checksum of the string
    for(charCnt = 0; charCnt < length; charCnt++) {
    	checksum ^= nmeaMsg[charCnt];
    }

    return checksum;
}

//*****************************************************************************
//
// Turn GPS On/Off
//
//*****************************************************************************
void ToggleGPS()
{
	    // turn PF3 off - toggle GPS
	    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x00);

	    // Wait 0.9 second (12M = 36M Cycles)
	    SysCtlDelay(16000000);

	    // turn PF3 on - toggle GPS
	    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x08);
}

//*****************************************************************************
//
// Initialize GPS to default message settings
//
//*****************************************************************************
void InitGPS()
{
	// Declare Variables
	uint32_t bufSize = 32;
	uint32_t nmeaChecksum;
	uint32_t loopCnt;
	uint32_t rate;


	char psrfCore[] = "PSRF103,%02u,%02u,%02u,01";
	char psrfWrap[] = "$%s*%x\r\n";

	// Turn on LED while we process button push
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02);

    // Allocate space for buffers
    char* psrfBuffer = malloc(bufSize);
    char* psrfFinal = malloc(bufSize);

    // Disable NMEA messages 0-4 (GGA,GLL,GSA,GSV)
    for(loopCnt = 0; loopCnt < 5; loopCnt++) {

    	/*if(loopCnt == 4)
    		rate = gpsIntervalMult * gpsIntervalSec * 0;
    	else
    		rate = 0;*/

    	// Above code is for letting GPS run. Now we turn off all messages when initializing
    	rate = 0;

    	// format the core string
    	sprintf(psrfBuffer, psrfCore, loopCnt, 0, rate);

    	// get checksum for nmea message
    	nmeaChecksum = NMEAChecksum(psrfBuffer);

    	// format the complete NMEA command string
    	sprintf(psrfFinal, psrfWrap, psrfBuffer, nmeaChecksum);

    	// Debug output to console
    	puts(psrfFinal);

    	SendToGPS(psrfFinal);

    }

    // Clean up heap
    free(psrfBuffer);
    free(psrfFinal);

    g_sGpsState.ui8InitState = 1;

    // turn led back off
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);

}
