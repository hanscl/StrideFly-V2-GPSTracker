//*****************************************************************************
//
// xbee.c - Tiva C Launchpad XBee Module for StrideFly Tracker
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
#include "xbee.h"
#include "stridefly.h"

//*****************************************************************************
//
// Global variables for XBee Radio
//
//*****************************************************************************
tXBeeState g_sXBeeState;
tXBeeDestAddr g_sXBeeDestAddr;

//*****************************************************************************
//
// Initialize XBee Radio
//
//*****************************************************************************
void InitXBee()
{
	uint32_t iCharCnt;
	uint32_t addrLen;
	char hex[3];

	// Allocate memory for the destination Address Strings
	g_sXBeeDestAddr.pHexHigh = malloc(9);
	g_sXBeeDestAddr.pHexLow = malloc(9);
	g_sXBeeDestAddr.pCharHigh = malloc(5);
	g_sXBeeDestAddr.pCharLow = malloc(5);

	// Set null terminator for strings
	g_sXBeeDestAddr.pCharHigh[4] = '\0';
	g_sXBeeDestAddr.pCharLow[4] = '\0';

	// Set dest address in hex (will come from flash later)
	char hexHigh[] = "0013A200";
	char hexLow[] = "40A7DB0F";

	// Copy the const chars into the strings on the heap
	ustrncpy(g_sXBeeDestAddr.pHexHigh, hexHigh, strlen(hexHigh));
	ustrncpy(g_sXBeeDestAddr.pHexLow, hexLow, strlen(hexLow));

	// Convert High Address to Chars
	addrLen = ustrlen(g_sXBeeDestAddr.pHexHigh);
	for(iCharCnt  = 0; iCharCnt < (addrLen -1)/2; iCharCnt++) {
		usprintf(hex, "%c%c", g_sXBeeDestAddr.pHexHigh[iCharCnt *2], g_sXBeeDestAddr.pHexHigh[iCharCnt *2 + 1]);
		g_sXBeeDestAddr.pCharHigh[iCharCnt] = (char)ustrtoul(hex, NULL, 16);
	}

	// Convert Low Address to Chars
	addrLen = ustrlen(g_sXBeeDestAddr.pHexLow);
	for(iCharCnt  = 0; iCharCnt < (addrLen -1)/2; iCharCnt++) {
		usprintf(hex, "%c%c", g_sXBeeDestAddr.pHexLow[iCharCnt *2], g_sXBeeDestAddr.pHexLow[iCharCnt *2 + 1]);
		g_sXBeeDestAddr.pCharLow[iCharCnt] = (char)ustrtoul(hex, NULL, 16);
	}

}

//*****************************************************************************
//
// Transmit message via XBee
//
//*****************************************************************************
void TransmitViaXBee(char *nmeaSentence)
{
	uint32_t iTxCnt;
	uint32_t checkSum;
	uint32_t iCharCnt;
	uint32_t currByte;

	char txFrame[XBEEBUFSIZE];

    // wake up Xbee by de-asserting PF2
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00);
    // Wait 500ms before continuing
    SysCtlDelay(1400000);

    for(iTxCnt = 1; iTxCnt <= g_sXBeeState.ui8TxRepeat; iTxCnt++) {
        currByte = 0;
        txFrame[currByte++] = 0x7E; // Start Delimiter
        txFrame[currByte++] = 0x00; // Placeholder for MSB Length
        txFrame[currByte++] = 0x00; // Placeholder for LSB Length
        txFrame[currByte++] = 0x10; // Begin Frame-specific data
        txFrame[currByte++] = 0x00; // Frame ID
        for(iCharCnt = 0; iCharCnt < 4; iCharCnt++) {
        	txFrame[currByte++] = g_sXBeeDestAddr.pCharHigh[iCharCnt];
        }
        for(iCharCnt = 0; iCharCnt < 4; iCharCnt++) {
        	txFrame[currByte++] = g_sXBeeDestAddr.pCharLow[iCharCnt];
        }
        txFrame[currByte++] = 0xFF; // MSB 16-bit Addr/Reserved
        txFrame[currByte++] = 0xFE; // LSB 16-bit Addr/Reserved
        txFrame[currByte++] = 0x00; // Radius
        txFrame[currByte++] = 0x00; // Options
        // Payload starts here
        txFrame[currByte++] = 0x5B; // '['
        txFrame[currByte++] = iTxCnt + 48; // Current Send Attempt
        txFrame[currByte++] = 0x02F; // '/'
        txFrame[currByte++] = g_sXBeeState.ui8TxRepeat + 48; // Total Send Attempts
        txFrame[currByte++] = 0x5D; // ']'
        for(iCharCnt = 0; iCharCnt < strlen(nmeaSentence); iCharCnt++) {
        	txFrame[currByte++] = nmeaSentence[iCharCnt];
         }

        // Update Length
        txFrame[2] = currByte - 3;

        // Calculate checksum
        checkSum = 0;
        for(iCharCnt = 3; iCharCnt < currByte; iCharCnt++)
        {
        	checkSum = (checkSum + (uint32_t)txFrame[iCharCnt]) % 256;
        }

        txFrame[currByte++] = 255 - checkSum;

        // Print final string to console
        //puts(txFrame);




     	// And put out HEX
        for(iCharCnt = 0; iCharCnt < currByte; iCharCnt++) {
        	printf("%02X ", txFrame[iCharCnt]);
        	UARTCharPut(UART2_BASE,txFrame[iCharCnt]);
        }

        // Wait 500ms before putting the module back to sleep
        SysCtlDelay(1400000);
        // put Xbee back to sleep
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x04);

    }
}
