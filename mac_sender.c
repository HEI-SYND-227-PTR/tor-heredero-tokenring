#include "main.h"
void MacSender(void *argument)
{
	// TODO
	struct queueMsg_t queueMsg;	// queue message
	char* msg;
	char msgToSend[255];
	uint8_t msgToSendPtr=0;
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
		
		switch(queueMsg.type) {
			
			case TOKEN:
				
				break;
			
			
			case DATABACK: 
				
				break;
			
			
			case NEW_TOKEN: 
				for(uint8_t i = 0; i < 15; i++) {
					if(i == gTokenInterface.myAddress) {
						gTokenInterface.station_list[i] = (0x1 << TIME_SAPI) & (gTokenInterface.connected << CHAT_SAPI);
					} else {
						gTokenInterface.station_list[i] = 0;
					}
				}
				msg = osMemoryPollAlloc(memPool, osWaitForever);
				queueMsg.type = TO_PHY;
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
