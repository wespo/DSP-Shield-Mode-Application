/* state machine extension for mailbox.cpp, allows one byte of a message to be read at a time rather than an entire message at once*/

#ifndef _MAILBOX_STATE_H_INCLUDED	//prevent mailbox library from being invoked twice and breaking the namespace
#define _MAILBOX_STATE_H_INCLUDED

#include "SPI.h"
#include "mailbox.h"
#include "core.h"

struct messageStateData {
  bool ongoingMessage;
  int byteCount;
  int messageLength;
  int checksum;
};

struct messageStateData initMessageStateData(messageStateData state);

int messageStateMachine(messageStateData &messageState);

#endif

