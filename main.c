#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "utils/ustdlib.h"

#define GPSBUFSIZE 128
#define XBEEBUFSIZE 256
#define FRAMEBUFSIZE 15

// Global Variables
uint32_t rcvCnt;	// GPS UART receive counter
const uint32_t gpsIntervalSec = 5;
uint32_t gpsIntervalMult;
uint32_t xbeeRepeat;
char *destAddrHexHigh;
char *destAddrHexLow;
char *destAddrCharHigh;
char *destAddrCharLow;



// Function Prototypes
void InitGPS(void);
uint32_t NMEAChecksum(char *nmeaMsg);
void SendToGPS(char *nmeaMsg);
void ToggleGPS(void);
void ProcessReceivedNMEA(char *nmeaSentence);
void TransmitViaXbee(char *nmeaSentence);
void InitXBee(void);


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
        	checkSum = (checkSum + (uint32_t)innerFrame[iCharCnt]) % 255;
        }

        innerFrame[currByte++] = 255 - checkSum;

        // Print final string to console
        puts(innerFrame);
        // And put out HEX
        for(iCharCnt = 0; iCharCnt < currByte; iCharCnt++) {
        	printf("%02X ", innerFrame[iCharCnt]);
        }

    }
}


void SendToGPS(char* nmeaMsg) {
	uint32_t charCnt, length;

	length = strlen(nmeaMsg);

	for(charCnt = 0; charCnt < length; charCnt++) {
		UARTCharPut(UART1_BASE,nmeaMsg[charCnt]);
	}
}

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

void ToggleGPS()
{
	    // turn PF3 off - toggle GPS
	    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x00);

	    // Wait 0.9 second (12M = 36M Cycles)
	    SysCtlDelay(12000000);

	    // turn PF3 on - toggle GPS
	    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0xFF);
}

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

    	if(loopCnt == 4)
    		rate = gpsIntervalMult * gpsIntervalSec;
    	else
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


    SysCtlDelay(2600000);

    // turn led back off
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);

}


void ProcessReceivedNMEA(char *nmeaSentence) {

	char gpsMsg[] = "$GPRMC";
	char cfgMsg[] = "$PSRF";

	char gpsReady[] = "$PSRF150,1";
	char gpsTrickle[] = "$PSRF150,0";


	puts("Received complete NMEA Sentence");
	printf("String Length: %u", strlen(nmeaSentence));
	puts(nmeaSentence);


	if(strncmp(nmeaSentence, cfgMsg, strlen(cfgMsg)) == 0) {
		puts("configuration message");
		if(strncmp(nmeaSentence, gpsReady, strlen(gpsReady)) == 0)
			InitGPS();
		else if(strncmp(nmeaSentence, gpsTrickle, strlen(gpsTrickle)) == 0)
			ToggleGPS();
	} else if(strncmp(nmeaSentence, gpsMsg, strlen(gpsMsg)) == 0) {
		puts("gps message");
		TransmitViaXBee(nmeaSentence);
	}

	puts("Processing Complete");
}

int main(void)
{
	char uartBuffer[GPSBUFSIZE];
	char nmeaStart = '$';
	char nmeaEnd = '\n';
	char* nmeaSentence;
    char nextChar;


    // Set CPU Clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Set default parameters
    gpsIntervalMult = 3;
    xbeeRepeat = 1;


    printf("Initializing Tiva C Launchpad\r\n");

    // Enable peripherals for Ports C, D, F and for UART1 and UART2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // Configure Pins PF1, PF2, PF3 for output
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

    // Initialize UART configuration (UART1 for GPS)
    GPIOPinConfigure(GPIO_PC4_U1RX);
    GPIOPinConfigure(GPIO_PC5_U1TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 4800, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Initialize UART configuration (UART2 for XBEE)
    GPIOPinConfigure(GPIO_PD6_U2RX);
    GPIOPinConfigure(GPIO_PD7_U2TX);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7);
    UARTConfigSetExpClk(UART2_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Turn on red LED
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0x02);

    // Init GPS
    ToggleGPS();
    InitXBee();

    printf("Finished initializing launchpad. Entering while loop.\r\n");

    // Turn on red LED
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0x00);

    // Initialize global receive counter as 0
    rcvCnt = 0;

    while(1)
    {
        // Process Chars from GPS at UART1 ...
        if (UARTCharsAvail(UART1_BASE)) {
        	nextChar = UARTCharGet(UART1_BASE);
        	putc(nextChar, stdout); // Debug
        	if(rcvCnt >= GPSBUFSIZE || nextChar == nmeaStart) { // ensure there is no buffer overflow
        		rcvCnt = 0;
        		uartBuffer[rcvCnt] = nextChar;
        	} else if (nextChar == nmeaEnd) { // process nmea sentence
        		// Save newline character & terminate string
        		uartBuffer[rcvCnt] = nextChar;
        		uartBuffer[++rcvCnt] = '\0';
        		// Allocate space on the heap & copy nmea string
        		nmeaSentence = malloc(rcvCnt);
        		memcpy(nmeaSentence, uartBuffer, rcvCnt+1);
        		printf("Rcv Counter: %u", rcvCnt); // Debug
        		// Process the GPS message
        		ProcessReceivedNMEA(nmeaSentence);
        		// Free up Heap
        		free(nmeaSentence);
        	} else {
        		// Keep saving chars to the buffer
        		uartBuffer[rcvCnt] = nextChar;
        	}

        	rcvCnt++;

        }
    }
}
