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
	osStatus_t retCode;					// return error code

	lastToken = osMemoryPoolAlloc(memPool, osWaitForever);
	queue_macData_id = osMessageQueueNew(4, sizeof(struct queueMsg_t), &queue_macData_attr);

	
	for(;;) {
		//----------------------------------------------------------------------------
		// QUEUE READ										
		//----------------------------------------------------------------------------
		retCode = osMessageQueueGet(
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		msg = queueMsg.anyPtr;
		
		switch(queueMsg.type) {
			
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

				// Send msg from internal queue if exist
				while (osMemoryPoolGetCount(queue_macData_id) != 0) { // Message in Queue
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
			
			
			case DATABACK: {

				break;
			}
			
			
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
			

			case START: {
				// Do nothing, don't care to receive start
				break;
			}
			

			case STOP: {
				// Do nothing, don't care to receive stop
				break;
			}
			

			case DATA_IND: {

				break;
			}

			
			default: {

				break;
			}
		}
		
	}
}
