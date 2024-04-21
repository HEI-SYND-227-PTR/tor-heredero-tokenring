#include "main.h"
#include <string.h>

uint8_t* lastToken;
osMessageQueueId_t queue_macData_id;
const osMessageQueueAttr_t queue_macData_attr = {
	.name = "MAC_DATA"
};

void MacSender(void *argument) {
	struct queueMsg_t queueMsg;	// queue message
	uint8_t* msg;
	Adresse src;
	Adresse dst;
	uint8_t length;
	Status status;
	osStatus_t retCode;					// return error code
	char* strPtr;

	lastToken = osMemoryPoolAlloc(memPool, osWaitForever);
	queue_macData_id = osMessageQueueNew(4, sizeof(struct queueMsg_t), &queue_macData_attr);

	
	for(;;) {
		//--------------------------------------------------------------------------
		// QUEUE READ										
		//--------------------------------------------------------------------------
		retCode = osMessageQueueGet(
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		msg = queueMsg.anyPtr;
		
		switch(queueMsg.type) {
			
			//----------------------------------------------------------------------
			// TOKEN MESSAGE
			//----------------------------------------------------------------------
			case TOKEN: {
				// Get token and save it
				memcpy(lastToken, msg, TOKENSIZE-2);

				// update token
				lastToken[gTokenInterface.myAddress+1] = (0x1 << TIME_SAPI) + (gTokenInterface.connected << CHAT_SAPI);
				for(uint8_t i = 1; i < sizeof(gTokenInterface.station_list); i++) {
					gTokenInterface.station_list[i-1] = lastToken[i];
				}

				// send to lcd
				queueMsg.type = TOKEN_LIST;
				queueMsg.anyPtr = lastToken;
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				// Send one msg from internal queue if exist
				if (osMemoryPoolGetCount(queue_macData_id) != 0) { // Message in Queue
					retCode = osMessageQueueGet(
						queue_macData_id,
						&queueMsg,
						NULL,
						osWaitForever);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				}

				// Send token
				queueMsg.anyPtr = lastToken;
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				break;
			}
			
			//----------------------------------------------------------------------
			// DATABACK MESSAGE
			//----------------------------------------------------------------------
			case DATABACK: {
				src.raw = msg[0];
				dst.raw = msg[1];
				length = msg[2];
				status.raw = msg[3+length];

				if(status.read == 0) {
					if(status.ack == 0) {
						msg = osMemoryPoolAlloc(memPool, osWaitForever);
						queueMsg.type = TO_PHY;
						queueMsg.anyPtr = 

					} else {

					}

				} else {
					strPtr = osMemoryPoolAlloc(memPool, osWaitForever);
					sprintf(strPtr, "Dest. %d couldn't read message from %d\0", dst.add+1, src.addr+1);
					queueMsg.type = MAC_ERROR;
					queueMsg.addr = src.addr;
					queueMsg.sapi = src.sapi;
					queueMsg.anyPtr = strPtr;
					retCode = osMessageQueuePut(
						queue_lcd_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				}
				break;
			}
			
			//----------------------------------------------------------------------
			// NEW TOKEN MESSAGE
			//----------------------------------------------------------------------
			case NEW_TOKEN: {
				lastToken[0] = TOKEN_TAG;
				
				for(uint8_t i = 1; i < sizeof(TOKENSIZE-2); i++) {
					lastToken[i] = 0;
				}
				gTokenInterface.station_list[gTokenInterface.myAddress] = (0x1 << TIME_SAPI) + (gTokenInterface.connected << CHAT_SAPI);
				lastToken[gTokenInterface.myAddress+1] = gTokenInterface.station_list[gTokenInterface.myAddress];
				
				queueMsg.type = TO_PHY;
				queueMsg.anyPtr = lastToken;
				
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				break;
			}
			
			//----------------------------------------------------------------------
			// START MESSAGE
			//----------------------------------------------------------------------
			case START: {
				gTokenInterface.connected = true;
				break;
			}
			
			//----------------------------------------------------------------------
			// STOP MESSAGE
			//----------------------------------------------------------------------
			case STOP: {
				gTokenInterface.connected = false;
				break;
			}
			
			//----------------------------------------------------------------------
			// DATA MESSAGE
			//----------------------------------------------------------------------
			case DATA_IND: {
				dst.addr = queueMsg.addr;
				dst.sapi = queueMsg.sapi;
				dst.nothing = 0;
				src.addr = gTokenInterface.myAddress;
				src.sapi = queueMsg.sapi;
				src.nothing = 0;
				length = strlen(queueMsg.anyPtr);

				if(dst.addr == BROADCAST_ADDRESS) {
					status.read = 1;
					status.ack = 1;
				} else {
					status.read = 0;
					status.ack = 0;
				}

				msg = osMemoryPoolAlloc(memPool, osWaitForever);
				msg[0] = src.raw;
				msg[1] = dst.raw;
				msg[2] = length;
				memcpy(&msg[3], queueMsg.anyPtr, length);
				status.checksum = Checksum(msg);
				msg[3+length] = status.raw;

				retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				queueMsg.anyPtr = msg;
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_macData_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				break;
			}

			//----------------------------------------------------------------------
			// DEFAULT - TBD
			//----------------------------------------------------------------------
			default: {
				break;
			}
		}
		
	}
}
