#include "messageStateMachine.h"

struct messageStateData initMessageStateData(messageStateData state)
{
  state.ongoingMessage = false;
  state.byteCount = 0;
  state.messageLength = 0;
  state.checksum = 0;
  return state;
}

int messageStateMachine(messageStateData &messageState) 
{
	//function handler that recieves and parses new messages, one byte per call
	//if no message is ongoing, simply returns
        //printMessageStateData(messageState);
	unsigned int spiWord;
	//check for ongoing message
	if(messageState.ongoingMessage)
	{
		//Yes, grab next byte. -- might be worth wrapping this next part in a check for the message hold down, but that will slow down message reciept.
		SPI.read(&spiWord, 1);
		messageState.byteCount++;
		if(messageState.byteCount == 1) //first message. Checksum
		{
			messageState.checksum = spiWord;
			return 0;
		}
		else if(messageState.byteCount == 2) //compute message length
		{
			messageState.messageLength = spiWord << 8;
			return 0;
		}
		else if(messageState.byteCount == 3)
		{
			messageState.messageLength += spiWord;
			if(messageState.messageLength > 2047) //reallocate the mailbox size.
			{
				messageState.messageLength = 2047; //enforce a reasonable maximum of 2kb on message size
			}
			shieldMailbox.inboxSize = messageState.messageLength;
			shieldMailbox.inbox = (char*) realloc(shieldMailbox.inbox, shieldMailbox.inboxSize);
			return 0;
		}
		else
		{
			shieldMailbox.inbox[messageState.byteCount - 4] = spiWord; //load read byte into the mailbox, offset because the first three bytes were 

			if(messageState.byteCount > messageState.messageLength + 2) //if this was the last byte;
			{
				//compute checksum
				unsigned int compChecksum = 0;
				for(unsigned int i = 0; i < shieldMailbox.inboxSize; i++)
				{
					compChecksum += shieldMailbox.inbox[i];
				}
				//validate checksum
				compChecksum &= 0x00FF;
				bool checksumValid = (messageState.checksum == compChecksum); //if the two checksums are equal, true.

				//reset all those magic flags
				messageState = initMessageStateData(messageState);

				if(checksumValid)
				{
					//send ack
			     	        spiWord = MB_ACK;
					SPI.write(&spiWord,1);
					//raise flag for handler
					return 1;

				}
				else //checksum not valid
				{
					//send bad ack
					spiWord = MB_BAD;
					SPI.write(&spiWord,1);
					//return without handler flag
					return 0;
				}
			}
			else //message not ready. Store word and move on.
			{
				return 0;
			}
		}		
	}
	else
	{
		//no, check if message start
		//bool interruptFlag = (CSL_CPU_REGS->IFR0) & //get flag here.

		//if(interruptFlag) //there has been an interrupt
                if(1)
		{
			//check if it's the SPI Master Selct pin(MS) held low.

                        int poll = digitalRead(MS);
                        //digitalWrite(LED2, LOW);
			if(poll == 0)
			{
                                //digitalWrite(LED2, LOW);
 				//Arduino signaled for message
				char sentinel;

				SPI.read((unsigned int *) &sentinel, 1);
				if(sentinel == '$') //if so, we have an ongoing message
				{
					messageState.ongoingMessage = true;
					messageState.byteCount = 0; //start the byte counter;
					return 0;
				}
				else //not a valid message.
				{
  					return 0;
				}

			}
else if(poll == 2)
{
    Wire.endTransmission();
    Wire.begin();
  asm(" BIT(ST1, #13) = #1"); //flag that we had an incident and try to recover from it.
  //delay(100);
  return 0;
    }
			else// it wasn't. Return and report no need to call the message handler.
			{
  				return 0;
			}
		}
		else //nope. No interrupt. We're good. Return and report no need to call the message handler
		{
  			return 0;
		}
	}



}
