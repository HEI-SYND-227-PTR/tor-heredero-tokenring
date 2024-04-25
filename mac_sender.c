#include "main.h"
#include <cstdio>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <cassert>

uint8_t* lastToken;
uint8_t* lastSentMsgPtr;

osMessageQueueId_t queue_macData_id;
const osMessageQueueAttr_t queue_macData_attr = {
	.name = "MAC_DATA"
};

void sendToken() {
	struct queueMsg_t queueMsg;
	queueMsg.anyPtr = lastToken;
	queueMsg.type = TO_PHY;
	osStatus_t retCode = osMessageQueuePut(
		queue_phyS_id,
		&queueMsg,
		osPriorityNormal,
		0);
	CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
}

void MacSender(void *argument) {
	struct queueMsg_t queueMsg;	// queue message
	uint8_t* msg;
	Adresse src;
	Adresse dst;
	uint8_t length;
	Status status;
	osStatus_t retCode;					// return error code
	char* strPtr;
	SapiToken stationStatus;

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
				//if (osMemoryPoolGetCount(queue_macData_id) != 0) { // Message in Queue
				retCode = osMessageQueueGet(queue_macData_id, &queueMsg, NULL, 0);
				//CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				if(retCode == 0){
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				} else {
					sendToken();
				}
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

				if (dst.addr == BROADCAST_ADDRESS) {
					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
					sendToken();
				} else if(src.addr != gTokenInterface.myAddress) {
					
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else if(status.read == 1) {
					if(status.ack == 1) {
						// Everything is fine, free memory
						retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						retCode = osMemoryPoolFree(memPool, lastSentMsgPtr);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						sendToken();

					} else {
						// Checksum error, send original message again
						if(lastSentMsgPtr != NULL) {
							//retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
							//CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

							memcpy(queueMsg.anyPtr, lastSentMsgPtr, lastSentMsgPtr[2]+4);
							queueMsg.type = TO_PHY;
							//queueMsg.anyPtr = lastSentMsgPtr;
							retCode = osMessageQueuePut(
								queue_phyS_id,
								&queueMsg,
								osPriorityNormal,
								0);
							CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

						} else {
							// Error, no original message found
							strPtr = osMemoryPoolAlloc(memPool, osWaitForever);
							sprintf(strPtr, "%d did shit on the ring #1\0", dst.addr);
							queueMsg.type = MAC_ERROR;
							queueMsg.addr = src.addr;
							queueMsg.sapi = src.sapi;
							queueMsg.anyPtr = strPtr;
							retCode = osMessageQueuePut(
								queue_lcd_id,
								&queueMsg,
								osPriorityNormal,
								0);
							CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						}
					}

				} else {
					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

					// Send error message to LCD
					strPtr = osMemoryPoolAlloc(memPool, osWaitForever);
					sprintf(strPtr, "Dest. %d couldn't read message from %d\0", dst.addr+1, src.addr+1);
					queueMsg.type = MAC_ERROR;
					queueMsg.addr = src.addr;
					queueMsg.sapi = src.sapi;
					queueMsg.anyPtr = strPtr;
					retCode = osMessageQueuePut(
						queue_lcd_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

					sendToken();
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

				if(dst.addr != BROADCAST_ADDRESS) {
					stationStatus.raw = gTokenInterface.station_list[dst.addr];
				}

					if( (stationStatus.chat == 1) || (dst.addr == BROADCAST_ADDRESS)) {
						
					if(dst.addr == BROADCAST_ADDRESS) {
						status.read = 1;
						status.ack = 1;
					} else {
						status.read = 0;
						status.ack = 0;
					}

					msg = osMemoryPoolAlloc(memPool, 0);
					if(msg == NULL) {
						printf("Memory allocation failed #1\r\n");
						assert(false);
					}
					msg[0] = src.raw;
					msg[1] = dst.raw;
					msg[2] = length;
					memcpy(&msg[3], queueMsg.anyPtr, length);
					status.checksum = Checksum(msg);
					msg[3+length] = status.raw;

					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);


					if( (dst.addr != BROADCAST_ADDRESS) && (dst.sapi == CHAT_SAPI) ) {
						lastSentMsgPtr = osMemoryPoolAlloc(memPool, 0);
						if(lastSentMsgPtr == NULL) {
							printf("Memory allocation failed #2\r\n");
							assert(false);
						}
						memcpy(lastSentMsgPtr, msg, length+4);
						// TODO test if station is online
					}

					queueMsg.anyPtr = msg;
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_macData_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else {
					strPtr = queueMsg.anyPtr;
					sprintf(strPtr, "%d is not online\0", dst.addr+1);
					queueMsg.type = MAC_ERROR;
					queueMsg.addr = src.addr;
					queueMsg.anyPtr = strPtr;
					retCode = osMessageQueuePut(
						queue_lcd_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				}

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
