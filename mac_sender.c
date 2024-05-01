#include "main.h"
#include <cstdio>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <cassert>

uint8_t* lastToken;			// Pointer to last token
uint8_t* lastSentMsgPtr;	// Pointer to last sent message (for retransmission)

// Queue for messages to be sent when token is received
osMessageQueueId_t queue_macData_id;
const osMessageQueueAttr_t queue_macData_attr = {
	.name = "MAC_DATA"
};

/**
 * @brief Send token to the next station
 */
void sendToken() {
	// Create queueMsg struct with new memory
	struct queueMsg_t queueMsg;
	queueMsg.anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	queueMsg.type = TO_PHY;

	// Copy token
	memcpy(queueMsg.anyPtr, lastToken, TOKENSIZE-2);

	// Send token to PHY
	osStatus_t retCode = osMessageQueuePut(
		queue_phyS_id,
		&queueMsg,
		osPriorityNormal,
		0);
	CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
}

/**
 * @brief MAC sender task
 */
void MacSender(void *argument) {
	struct queueMsg_t queueMsg;	// queue message
	uint8_t* msg;				// pointer to message
	Adresse src;			    // source Address
	Adresse dst;			    // destination Address
	uint8_t length;				// length of message
	Status status;				// status of message
	osStatus_t retCode;			// return error code
	char* strPtr;				// pointer to string message
	SapiToken stationStatus;	// Status of one station

	// Allocate memory for last token
	lastToken = osMemoryPoolAlloc(memPool, osWaitForever);

	// Create queue for messages to be sent when token is received
	queue_macData_id = osMessageQueueNew(4, sizeof(struct queueMsg_t), &queue_macData_attr);

	
	for(;;) {
		//--------------------------------------------------------------------------
		// QUEUE READ										
		//--------------------------------------------------------------------------
		{
		// Get message from queue, test retCode and get msg
		retCode = osMessageQueueGet(
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		msg = queueMsg.anyPtr;
		}
		
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
				memcpy(queueMsg.anyPtr , lastToken, TOKENSIZE-2);
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				
				// Free memory 
				retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				// Send one msg from internal queue if exist
				retCode = osMessageQueueGet(queue_macData_id, &queueMsg, NULL, 0);
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
				// Get source Addresse, destination Addresse, length and status
				src.raw = msg[0];
				dst.raw = msg[1];
				length = msg[2];
				status.raw = msg[3+length];

				if (dst.addr == BROADCAST_ADDRESS) {
					// Broadcast message -> free memory
					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

					// Send token
					sendToken();

				} else if(src.addr != gTokenInterface.myAddress) {
					// Not from me -> to PHY
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else if(status.read == 1) {
					if(status.ack == 1) {
						// Read + ack => Everything is fine -> free memory and send token
						retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						retCode = osMemoryPoolFree(memPool, lastSentMsgPtr);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						sendToken();

					} else {
						// Read but checksum error, send original message again
						if(lastSentMsgPtr != NULL) {
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
					// Not read => Station not connected -> free backup message
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

					// Send token
					sendToken();
				}

				break;
			}
			
			//----------------------------------------------------------------------
			// NEW TOKEN MESSAGE
			//----------------------------------------------------------------------
			case NEW_TOKEN: {
				// Create new token
				lastToken[0] = TOKEN_TAG;
				
				// Set all station status to 0
				memset(lastToken, 0, sizeof(TOKENSIZE-2));
				
				// Set my station status on station list and lastToken
				gTokenInterface.station_list[gTokenInterface.myAddress] = (0x1 << TIME_SAPI) + (gTokenInterface.connected << CHAT_SAPI);
				lastToken[gTokenInterface.myAddress+1] = gTokenInterface.station_list[gTokenInterface.myAddress];

				// Send token
				sendToken();
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
				// Set source Addresse, destination Addresse and length
				dst.addr = queueMsg.addr;
				dst.sapi = queueMsg.sapi;
				dst.nothing = 0;
				src.addr = gTokenInterface.myAddress;
				src.sapi = queueMsg.sapi;
				src.nothing = 0;
				length = strlen(queueMsg.anyPtr);

				// Set station status
				if(dst.addr == BROADCAST_ADDRESS) {
					status.read = 1;
					status.ack = 1;
					stationStatus.raw = 0;
				} else {
					status.read = 0;
					status.ack = 0;
					stationStatus.raw = gTokenInterface.station_list[dst.addr];
				}

				// Check if destination is online
				if( (dst.addr == BROADCAST_ADDRESS) || (stationStatus.chat == 1)) {

					// Allocate memory for message and check if allocation was successful
					msg = osMemoryPoolAlloc(memPool, 0);
					if(msg == NULL) {
						printf("Memory allocation failed #1\r\n");
						assert(false);
					}

					// Set message
					msg[0] = src.raw;
					msg[1] = dst.raw;
					msg[2] = length;

					// Copy message to memory
					memcpy(&msg[3], queueMsg.anyPtr, length);

					// Set status
					status.checksum = Checksum(msg);
					msg[3+length] = status.raw;

					// Free memory
					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

					// Backup message if destination is chat station and isn't a broadcast message
					if( (dst.addr != BROADCAST_ADDRESS) && (dst.sapi == CHAT_SAPI) ) {
						lastSentMsgPtr = osMemoryPoolAlloc(memPool, 0);
						if(lastSentMsgPtr == NULL) {
							printf("Memory allocation failed #2\r\n");
							assert(false);
						}
						memcpy(lastSentMsgPtr, msg, length+4);
					}

					// Send message to PHY
					queueMsg.anyPtr = msg;
					queueMsg.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_macData_id,
						&queueMsg,
						osPriorityNormal,
						0);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				} else {
					// Destination is not online
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
