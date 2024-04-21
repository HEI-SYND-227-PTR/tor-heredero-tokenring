
#include "main.h"
#include <cassert>
#include <cstdint>
#include <stdint.h>

void send_DATA_IND(Adresse source, Adresse destination, uint8_t* dataFramePtr) {
	struct queueMsg_t queueMsg;	// queue message
	osStatus_t retCode;			// return error code
	char* strPtr; 

	queueMsg.type = DATA_IND;
	queueMsg.addr = source.addr;
	queueMsg.sapi = source.sapi;

	strPtr = osMemoryPoolAlloc(memPool, 0);
	if(strPtr == NULL) {
		assert(false);
	}

	for(uint8_t i = 0; i < dataFramePtr[2]; i++) {
		strPtr[i] = (char)dataFramePtr[3+i];
	}
	strPtr[dataFramePtr[2]] = '\0'; // null-terminate string
	queueMsg.anyPtr = strPtr;

	switch (destination.sapi) {
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

void send_DATABACK(Adresse source, Adresse destination, uint8_t* dataFramePtr) {
	struct queueMsg_t queueMsg;	// queue message
	osStatus_t retCode;			// return error code

	queueMsg.type = DATABACK;
	queueMsg.anyPtr = dataFramePtr;
	queueMsg.addr = source.addr;
	queueMsg.sapi = source.sapi;


	retCode = osMessageQueuePut(
		queue_macS_id,
		&queueMsg,
		osPriorityNormal,
		0);
	CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
	
}


void MacReceiver(void *argument) {
	struct queueMsg_t queueMsg;	// queue message
	Adresse src;
	Adresse dst;
	uint8_t length;
	Status status;
	uint8_t* msg;
	osStatus_t retCode;			// return error code

	for(;;) {
		//--------------------------------------------------------------------------
		// QUEUE READ										
		//--------------------------------------------------------------------------
		{
		retCode = osMessageQueueGet(
			queue_macR_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		msg = queueMsg.anyPtr;
		}
		switch (queueMsg.type) {

			//----------------------------------------------------------------------
			// MESSAGE FROM PHY
			//----------------------------------------------------------------------
			case FROM_PHY:
				if(msg[0] == TOKEN_TAG) {
					//--------------------------------------------------------------
					// TOKEN
					//--------------------------------------------------------------
					queueMsg.type = TOKEN;
					retCode = osMessageQueuePut(
						queue_macS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
						printf("Token received\r\n");
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else {
					//--------------------------------------------------------------
					// MESSAGE
					//--------------------------------------------------------------
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
							status.ack = 1;
							
							if(dst.sapi == CHAT_SAPI && gTokenInterface.connected ||
								dst.sapi == TIME_SAPI && gTokenInterface.broadcastTime) {
								// Send to Time or Chat ----------------------------
								send_DATA_IND(src, dst, queueMsg.anyPtr);
								status.read = 1;
							} else {
								status.read = 0;
							}
							msg[3+length] = status.raw;
							
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
							status.read = 0;
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
