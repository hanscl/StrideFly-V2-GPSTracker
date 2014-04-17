//*****************************************************************************
//
// stridefly.c - Tiva C Launchpad GPS/XBee Tracker Module by StrideFly
//
// Copyright (c) 2014 StrideFly, LLC.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

// C-Library standard includes
#include <stdint.h>
#include <stdlib.h>  // Need this and ustdlib?
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "utils/ustdlib.h"
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
#include "stridefly.h"
#include "xbee.h"
#include "gps.h"

//*****************************************************************************
//
// main function
//
// Initialize launchpad and enter while loop to read GPS messages when available
//
//*****************************************************************************
int main(void)
{
	char uartBuffer[GPSBUFSIZE];
	char nmeaStart = '$';
	char nmeaEnd = '\n';
	char *nmeaSentence;
    char nextChar;

      // Set CPU Clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Set default parameters for GPS & XBee
    g_sGpsState.ui8IntervalSec = 15;  // Query GPS every 15 seconds
    g_sGpsState.ui8InitState = 0;  // GPS has not been initialized; used to be bInitGPS = 1;
    g_sGpsState.i8OnOff = -1;	// GPS On/Off state is unknown; used to be gpsOnOff = 99;

    g_sXBeeState.ui8TxRepeat = 1; // Set # of transmissions for XBee Radio

    printf("Initializing Tiva C Launchpad\r\n");  // DEBUG OUTPUT

    // Enable peripherals for Ports C, D, F and for UART1/UART2 and TIMER0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    // Configure Pins PF1, PF2, PF3 for output
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

    // Initialize UART configuration (UART1 for GPS)
    GPIOPinConfigure(GPIO_PC4_U1RX);
    GPIOPinConfigure(GPIO_PC5_U1TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 4800, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    //***********************************************
    // Initialize UART configuration (UART2 for XBEE)
    //***********************************************
    // Enable pin PD7 for UART2 U2TX
    // First open the lock and select the bits we want to modify in the GPIO commit register.
    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTD_BASE + GPIO_O_CR) = 0x80;
    // Pin configure and UART init
    GPIOPinConfigure(GPIO_PD6_U2RX);
    GPIOPinConfigure(GPIO_PD7_U2TX);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7);
    UARTConfigSetExpClk(UART2_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Configure Timer
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    uint32_t ui32Period = (SysCtlClockGet() * g_sGpsState.ui8IntervalSec); // Remove this and consolidate into next line
    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32Period -1);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Enable Interrupts
    IntMasterEnable();

    // Enable the timer
    TimerEnable(TIMER0_BASE, TIMER_A);

    // Always initialize the XBee Radio upon restart
    InitXBee();

	// Toggle the GPS; this will trigger a PSRF message telling us if the module was turned on or off
    // we can then make sure that the module is initialized properly
	// This should only happen once. After that we will know the state (set in gps.c) and can ensure that the GPS module is initialized
    ToggleGPS();

    printf("Finished initializing launchpad. Entering while loop.\r\n");  // DEBUG OUTPUT

    // Initialize GPS receive counter with 0
    g_sGpsState.ui8CharRcvCnt = 0; // used to be rcvCnt = 0;

    // Enter the infinte while-loop
    while(1)
    {
    	// Check here if the GPS has been initialized; if not we need to either initialize it or turn it on first
    	if(g_sGpsState.ui8InitState == 0) {
    		if(g_sGpsState.i8OnOff == 1)
    			InitGPS();
    		else if(g_sGpsState.i8OnOff == 0)
    			ToggleGPS();
    	}

        // Process Chars from GPS at UART1 ...
        if (UARTCharsAvail(UART1_BASE)) {
        	nextChar = UARTCharGet(UART1_BASE);
        	putc(nextChar, stdout); // DEBUG OUTPUT

        	// Reset char counter back to zero if nmeaStart ($) is received OR when the buffer would overflow
        	if(g_sGpsState.ui8CharRcvCnt >= GPSBUFSIZE || nextChar == nmeaStart) {
        		g_sGpsState.ui8CharRcvCnt = 0;
        		uartBuffer[g_sGpsState.ui8CharRcvCnt] = nextChar;

        	// If the NMEA sentence is completed; terminate the string, copy it into the heap and pass the
        	// address over into gps.c for processing
        	} else if (nextChar == nmeaEnd) {
        		uartBuffer[g_sGpsState.ui8CharRcvCnt] = nextChar;
        		uartBuffer[++g_sGpsState.ui8CharRcvCnt] = '\0';

        		// Allocate space on the heap & copy nmea string
        		nmeaSentence = malloc(g_sGpsState.ui8CharRcvCnt);
        		memcpy(nmeaSentence, uartBuffer, g_sGpsState.ui8CharRcvCnt+1); // WHY IS THIS PLUS ONE???
        		printf("Rcv Counter: %u", g_sGpsState.ui8CharRcvCnt); // DEBUG OUTPUT

        		// Process the GPS message
        		ProcessReceivedNMEA(nmeaSentence);

        		// Free up Heap once we continue execution here
        		free(nmeaSentence);

        	// Otherwise just keep saving the chars
        	} else {

        		uartBuffer[g_sGpsState.ui8CharRcvCnt] = nextChar;
        	}

        	g_sGpsState.ui8CharRcvCnt++;

        }

        // If there are any chars available at UART2/XBEE, get them out ...
        if (UARTCharsAvail(UART2_BASE)) {
        	nextChar = UARTCharGet(UART2_BASE);  // Do we need to assign to nextChar?
            putc(nextChar, stdout); // Remove this
        }
    }
}

//*****************************************************************************
//
// Timer function called periodically to enable GPS
// GPS ready message will trigger query for GPRMC (see gps.c)
//
//*****************************************************************************
void Timer0IntHandler(void)
{
	// Clear timer interrupts
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	// If the GPS is not initialized yet, just exit
	// This will be handled by the while() loop inside main.
	if(g_sGpsState.ui8InitState == 0)
		return;

	// If the GPS is off, turn it on.  The module will automatically be queried
	// inside gps.c after it has issued a PSRF150 ready message
	// If the module is already on, we need to query the GPS from here. Otherwise it would never
	// be queried (or put back into hibernation, for that matter)
	if(g_sGpsState.i8OnOff == 0)
		ToggleGPS();
	else if(g_sGpsState.i8OnOff == 1)
		QueryGPS();
}
