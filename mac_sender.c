#include "main.h"
void MacSender(void *argument)
{
	// TODO
	struct queueMsg_t queueMsg;	// queue message
	uint8_t* msg;
	osStatus_t retCode;					// return error code
	
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
			
			case TOKEN:
				msg[gTokenInterface.myAddress] = gTokenInterface.station_list[gTokenInterface.myAddress];
				for(uint8_t i = 1; i < TOKENSIZE-2; i++) {
					gTokenInterface.station_list[i] = msg[i];
				}
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				break;
			
			
			case DATABACK: 
				
				break;
			
			
			case NEW_TOKEN: 
				msg = osMemoryPoolAlloc(memPool, osWaitForever);
				msg[0] = TOKEN_TAG;
				
				for(uint8_t i = 1; i < sizeof(TOKENSIZE-2); i++) {
					msg[i] = 0;
				}
				gTokenInterface.station_list[gTokenInterface.myAddress] = (0x1 << TIME_SAPI) + (gTokenInterface.connected << CHAT_SAPI);
				msg[gTokenInterface.myAddress+1] = gTokenInterface.station_list[gTokenInterface.myAddress];
				
				queueMsg.type = TO_PHY;
				queueMsg.anyPtr = msg;
				
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				break;
			
			
			case START:
				
				break;
			
			
			case STOP:
				
				break;
			
			
			case DATA_IND:
				
				break;
			
			
			default:
				
				break;
		}
		
	}
}
