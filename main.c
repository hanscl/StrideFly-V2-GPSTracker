

//#include <stdbool.h>






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















