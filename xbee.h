/*
 * xbee.h
 *
 *  Created on: Apr 16, 2014
 *      Author: Hans
 */

#ifndef XBEE_H_
#define XBEE_H_

//*****************************************************************************
//
// Structure typedef to make storing application state data simpler.
//
//*****************************************************************************
typedef struct
{
	uint8_t ui8CharRcvCnt; // Chars received from XBee UART
	uint8_t ui8TxRepeat; // Number of times to transmit from XBee

}tXBeeState;

typedef struct
{
	char *pHexHigh;
	char *pHexLow;
	char *pCharHigh;
	char *pCharLow;

}tXBeeDestAddr;


//*****************************************************************************
//
// Global variables originally defined in xbee.c that are made available to
// functions in other files.
//
//*****************************************************************************
extern tXBeeState g_sXBeeState;

//*****************************************************************************
//
// Functions defined in xbee.c that are made available to other files.
//
//*****************************************************************************
extern void InitXBee(void);
extern void TransmitViaXbee(char *nmeaSentence);

#endif /* XBEE_H_ */
