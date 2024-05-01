
#include "main.h"
#include <cassert>
#include <cstdint>
#include <stdint.h>

/**
 * @brief Senda copy of DATA_IND to the right application queue
 * 
 * @param source source Address (addr, sapi)
 * @param destination destination Address (addr, sapi)
 * @param dataFramePtr pointer to the data frame
 
*/
void send_DATA_IND(Adresse source, Adresse destination, uint8_t* dataFramePtr) {
	struct queueMsg_t queueMsg;	// queue message
	osStatus_t retCode;			// return error code
	char* strPtr; 				// string pointer of the message

	// Get memmory for new message and test if succed
	strPtr = osMemoryPoolAlloc(memPool, 0);
	if(strPtr == NULL) {
		assert(false);
	}

	// Copy data from dataFramePtr to strPtrt8_t* dataFramePtr) {
	for(uint8_t i = 0; i < dataFramePtr[2]; i++) {
		strPtr[i] = (char)dataFramePtr[3+i];
	}

	// add null-terminate string
	strPtr[dataFramePtr[2]] = '\0';
	
	// Define data in queueMsg struct
	queueMsg.type = DATA_IND;
	queueMsg.addr = source.addr;
	queueMsg.sapi = source.sapi;
	queueMsg.anyPtr = strPtr;

	// Test distination
	switch (destination.sapi) {

		// Send to right application queue
		case TIME_SAPI:
			retCode = osMessageQueuePut(
				queue_timeR_id,
				&queueMsg,
				osPriorityNormal,
				0);
			CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
			break;
		case CHAT_SAPI:
			retCode = osMessageQueuePut(
				queue_chatR_id,
				&queueMsg,
				osPriorityNormal,
				0);
			CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
			break;
		default:
			assert(false);
	}
}

/**
 * @brief Send DATABACK to MAC sender
 * 
 * @param source source Address (addr, sapi)
 * @param destination destination Address (addr, sapi)
 * @param dataFramePtr pointer to the data frame
 */
void send_DATABACK(Adresse source, Adresse destination, uint8_t* dataFramePtr) {
	struct queueMsg_t queueMsg;	// queue message
	osStatus_t retCode;			// return error code

	// Define data in queueMsg struct
	queueMsg.type = DATABACK;
	queueMsg.anyPtr = dataFramePtr;
	queueMsg.addr = source.addr;
	queueMsg.sapi = source.sapi;

	// Put on MAC sender queue
	retCode = osMessageQueuePut(
		queue_macS_id,
		&queueMsg,
		osPriorityNormal,
		0);
	CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
}


void MacReceiver(void *argument) {
	struct queueMsg_t queueMsg;	// queue message
	Adresse src;				// source Address (addr, sapi)
	Adresse dst;				// destination Address (addr, sapi)
	uint8_t length;
	Status status;
	uint8_t* msg;
	osStatus_t retCode;			// return error code

	for(;;) {
		//--------------------------------------------------------------------------
		// QUEUE READ										
		//--------------------------------------------------------------------------
		{
		// Get message from queue, test retCode and get msg
		retCode = osMessageQueueGet(
			queue_macR_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		msg = queueMsg.anyPtr;
		}

		//--------------------------------------------------------------------------
		// SWITCH ON MESSAGE TYPE
		//--------------------------------------------------------------------------
		switch (queueMsg.type) {

			//----------------------------------------------------------------------
			// MESSAGE FROM PHY
			//----------------------------------------------------------------------
			case FROM_PHY:
				if(msg[0] == TOKEN_TAG) {
					//--------------------------------------------------------------
					// TOKEN
					//--------------------------------------------------------------
					// Send token to MAC sender
					queueMsg.type = TOKEN;
					retCode = osMessageQueuePut(
						queue_macS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else {
					//--------------------------------------------------------------
					// MESSAGE
					//--------------------------------------------------------------
					// Get source Addresse, destination Addresse, length and status
					src.raw = msg[0];
					dst.raw = msg[1];
					length = msg[2];
					status.raw = msg[3+length];

					//--------------------------------------------------------------
					// MESSAGE FOR ME (or broadcast)
					//--------------------------------------------------------------
					if( dst.addr == gTokenInterface.myAddress ||
						dst.addr == BROADCAST_ADDRESS ) {
						
						if((Checksum(msg) & 0x3F) == status.checksum) {
							// Checksum OK -----------------------------------------
							status.ack = 1;
							
							if(dst.sapi == CHAT_SAPI && gTokenInterface.connected ||
								dst.sapi == TIME_SAPI) {
								// Send to Time or Chat ----------------------------
								send_DATA_IND(src, dst, queueMsg.anyPtr);
								status.read = 1;
							} else {
								status.read = 0;
							}
							msg[3+length] = status.raw; // Add status to message
							
							if(src.addr == gTokenInterface.myAddress) { // For me, from me
								// Send DATABACK -----------------------------------
								send_DATABACK(src, dst, queueMsg.anyPtr);
							} else {
								// Send to PHY -------------------------------------
								queueMsg.type = TO_PHY;
								retCode = osMessageQueuePut(
									queue_phyS_id,
									&queueMsg,
									osPriorityNormal,
									0);
								CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
							}
						
						} else { // for me but bad checksum
							status.ack = 0;
							status.read = gTokenInterface.connected; // Maybe it's 1
							msg[3+length] = status.raw;
							send_DATABACK(src, dst, queueMsg.anyPtr);
						}

					//--------------------------------------------------------------
					// MESSAGE FOR SOMEONE ELSE
					//--------------------------------------------------------------
					} else if(src.addr == gTokenInterface.myAddress) {
						// MESSAGE FROM ME -----------------------------------------
						send_DATABACK(src, dst, queueMsg.anyPtr);
					} else {
						// MESSAGE FROM SOMEONE ELSE -------------------------------
						queueMsg.type = TO_PHY;
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							0);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
					}
					
				}
				break;
			
			//----------------------------------------------------------------------
			// DEFAULT - TBD
			//----------------------------------------------------------------------
			default:
				break;
			}
	}
	
}
