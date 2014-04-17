#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
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
#include "utils/ustdlib.h"



// Global Variables
uint32_t rcvCnt, xbeeRcv;	// GPS UART receive counter
const uint32_t gpsIntervalSec = 5;
uint32_t gpsIntervalMult;
uint32_t xbeeRepeat;
char *destAddrHexHigh;
char *destAddrHexLow;
char *destAddrCharHigh;
char *destAddrCharLow;
uint32_t bInitGPS;
uint32_t gpsOnOff;


// Function Prototypes















