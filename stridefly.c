//*****************************************************************************
//
// stridefly.c - Tiva C Launchpad GPS/XBee Tracker Module by StrideFly
//
// Copyright (c) 2014 StrideFly, LLC.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

#include "stridefly.h"

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
	char xbeeBuffer[XBEEBUFSIZE];
	char nmeaStart = '$';
	char nmeaEnd = '\n';
	char* nmeaSentence;
    char nextChar;


    uint32_t ui32Period;


    // Set CPU Clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Set default parameters
    gpsIntervalMult = 3;
    xbeeRepeat = 1;
    bInitGPS = 1;
    gpsOnOff = 99;


    printf("Initializing Tiva C Launchpad\r\n");

    // Enable peripherals for Ports C, D, F and for UART1 and UART2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    // Enable timer
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);



    // Configure Pins PF1, PF2, PF3 for output
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

    // Initialize UART configuration (UART1 for GPS)
    GPIOPinConfigure(GPIO_PC4_U1RX);
    GPIOPinConfigure(GPIO_PC5_U1TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 4800, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    //
       // Enable pin PD7 for UART2 U2TX
       // First open the lock and select the bits we want to modify in the GPIO commit register.
       //
       HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
       HWREG(GPIO_PORTD_BASE + GPIO_O_CR) = 0x80;

    // Initialize UART configuration (UART2 for XBEE)
    GPIOPinConfigure(GPIO_PD6_U2RX);
    GPIOPinConfigure(GPIO_PD7_U2TX);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_6 | GPIO_PIN_7);
    UARTConfigSetExpClk(UART2_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Configure Timer
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

   // uint32_t test = SysCtlClockGet();
    ui32Period = (SysCtlClockGet() * 10);
    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32Period -1);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);


    // Turn on red LED
   // GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0x02);


    InitXBee();

    printf("Finished initializing launchpad. Entering while loop.\r\n");

    // Turn on red LED
   // GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0x00);

    // Initialize global receive counter as 0
    rcvCnt = 0;
    xbeeRcv = 0;



    while(1)
    {
        if(gpsOnOff == 99) { // on/off status is unknown
        	ToggleGPS();
        	gpsOnOff = 88;
        }

        if(gpsOnOff == 1 && bInitGPS == 1) // gps is on but hasn't been initialized yet
        	InitGPS();

        if(gpsOnOff == 0 && bInitGPS == 1) // gps is off and hasn't been initialized. turn it on first
        	ToggleGPS();

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

        // Process Chars from GPS at UART1 ...
               if (UARTCharsAvail(UART2_BASE)) {
               	nextChar = UARTCharGet(UART2_BASE);
               	putc(nextChar, stdout); // Debug


               	xbeeRcv++;

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
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	if(bInitGPS == 1) // has not been initialized. Return
		return;

	if(gpsOnOff = 0);
		ToggleGPS();


	//ToggleGPS();
	//SysCtlDelay(10000000); // wait for the GPS to wake up
	//QueryGPS();
}
