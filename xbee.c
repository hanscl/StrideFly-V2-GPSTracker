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
#include "xbee.h"

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
	destAddrHexHigh = malloc(9);
	destAddrHexLow = malloc(9);
	destAddrCharHigh = malloc(5);
	destAddrCharLow = malloc(5);

	// Set null terminator for strings
	destAddrCharHigh[4] = '\0';
	destAddrCharLow[4] = '\0';

	// Set dest address in hex (will come from flash later)
	char hexHigh[] = "0013A200";
	char hexLow[] = "40A7DB0F";

	// Copy the const chars into the strings on the heap
	ustrncpy(destAddrHexHigh, hexHigh, strlen(hexHigh));
	ustrncpy(destAddrHexLow, hexLow, strlen(hexLow));

	// Convert High Address to Chars
	addrLen = ustrlen(destAddrHexHigh);
	for(iCharCnt  = 0; iCharCnt < (addrLen -1)/2; iCharCnt++) {
		usprintf(hex, "%c%c", destAddrHexHigh[iCharCnt *2], destAddrHexHigh[iCharCnt *2 + 1]);
		destAddrCharHigh[iCharCnt] = (char)ustrtoul(hex, NULL, 16);
	}

	// Convert Low Address to Chars
	addrLen = ustrlen(destAddrHexLow);
	for(iCharCnt  = 0; iCharCnt < (addrLen -1)/2; iCharCnt++) {
		usprintf(hex, "%c%c", destAddrHexLow[iCharCnt *2], destAddrHexLow[iCharCnt *2 + 1]);
		destAddrCharLow[iCharCnt] = (char)ustrtoul(hex, NULL, 16);
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
	uint32_t byteCount;
	uint32_t cnt[3];
	uint32_t currByte;

	char innerFrame[XBEEBUFSIZE];
    char payload[GPSBUFSIZE];
    char txFrame[XBEEBUFSIZE];

    // wake up Xbee by de-asserting PF2
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00);
    // Wait 500ms before continuing
            SysCtlDelay(1400000);

    for(iTxCnt = 1; iTxCnt <= xbeeRepeat; iTxCnt++) {
        currByte = 0;
        innerFrame[currByte++] = 0x7E; // Start Delimiter
        innerFrame[currByte++] = 0x00; // Placeholder for MSB Length
        innerFrame[currByte++] = 0x00; // Placeholder for LSB Length
        innerFrame[currByte++] = 0x10; // Begin Frame-specific data
        innerFrame[currByte++] = 0x00; // Frame ID
        for(iCharCnt = 0; iCharCnt < 4; iCharCnt++) {
        	innerFrame[currByte++] = destAddrCharHigh[iCharCnt];
        }
        for(iCharCnt = 0; iCharCnt < 4; iCharCnt++) {
        	innerFrame[currByte++] = destAddrCharLow[iCharCnt];
        }
        innerFrame[currByte++] = 0xFF; // MSB 16-bit Addr/Reserved
        innerFrame[currByte++] = 0xFE; // LSB 16-bit Addr/Reserved
        innerFrame[currByte++] = 0x00; // Radius
        innerFrame[currByte++] = 0x00; // Options
        // Payload starts here
        innerFrame[currByte++] = 0x5B; // '['
        innerFrame[currByte++] = iTxCnt + 48; // Current Send Attempt
        innerFrame[currByte++] = 0x02F; // '/'
        innerFrame[currByte++] = xbeeRepeat + 48; // Total Send Attempts
        innerFrame[currByte++] = 0x5D; // ']'
        for(iCharCnt = 0; iCharCnt < strlen(nmeaSentence); iCharCnt++) {
        	innerFrame[currByte++] = nmeaSentence[iCharCnt];
         }

        // Update Length
        innerFrame[2] = currByte - 3;

        // Calculate checksum
        checkSum = 0;
        for(iCharCnt = 3; iCharCnt < currByte; iCharCnt++)
        {
        	checkSum = (checkSum + (uint32_t)innerFrame[iCharCnt]) % 256;
        }

        innerFrame[currByte++] = 255 - checkSum;

        // Print final string to console
        //puts(innerFrame);




     	// And put out HEX
        for(iCharCnt = 0; iCharCnt < currByte; iCharCnt++) {
        	printf("%02X ", innerFrame[iCharCnt]);
        	UARTCharPut(UART2_BASE,innerFrame[iCharCnt]);
        }

        // Wait 500ms before putting the module back to sleep
        SysCtlDelay(1400000);
        // put Xbee back to sleep
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x04);

    }
}
