//=============================================================================
//	This file is part of MCSB, the Multi-Client Shared Buffer
//	Copyright (C) 2014  Gregory E. Allen and
//		Applied Research Laboratories: The University of Texas at Austin
//
//	MCSB is free software: you can redistribute it and/or modify it
//	under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 2.1 of the License, or
//	(at your option) any later version.
//
//	MCSB is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//	Lesser General Public License <http://www.gnu.org/licenses/lgpl.html>
//	for more details.
//=============================================================================

// This file is used in examples 2-4. Please start with example 1.

// Both definition and implementation are included in this single file.
// Normally, that's bad form.
// However, I do it here because it makes the example much easier to follow.

#ifndef DescriptorHello_h
#define DescriptorHello_h
#pragma once

#include "MCSB/Client.h"

#include <cstdio>
#include <unistd.h>
#include <ev++.h>

// This is similar to SimpleHello in ex1, except uses MessageDescriptors to send
// and receive a message. See ex2_DescriptorHello.cc for further explanation.
class DescriptorHello {
  public:
	// This method will be called back upon receipt of a message.
	// This is the descriptor-based function signature for an MCSB message callback.
	void HandleMessage(const MCSB::RecvMessageDescriptor& desc)
	{
		// all the same information as before is available in the RecvMessageDescriptor
		uint32_t msgID = desc.MessageID();
		uint32_t len = desc.Nbytes();
		const void* msg = desc.Buf();
		// msg points directly into shared memory, and no copy was needed to move
		// this message into a user's buffer

		printf("%s\n", (const char*)msg);

		// re-send the same message (with the "traditional" copying interface)
		client.SendMessage(msgID,msg,len);
		// we sleep to slow down the messages
		// (but you would never do this in real application)
		usleep(100000);
		
		// if a count was specified and we've reached it
		msgsRcvd++;
		if (exitCount && msgsRcvd>=exitCount) {
			printf("reached message count, exiting\n");
			// then tell libev to exit the main loop
			ev::default_loop loop;
			loop.break_loop();
		}
		
		// a reminder for the user
		if (!exitCount)
			printf("(control-C to exit)\n");
	}

	// This constructor will install a message handler and send a message.
	DescriptorHello(MCSB::Client& client_, uint32_t msgID_, unsigned count=0)
		: client(client_), msgID(msgID_), exitCount(count), msgsRcvd(0)
	{
		// RegisterForMsgID using RecvMessageDescriptor signature.
		client.RegisterForMsgID<DescriptorHello,&DescriptorHello::HandleMessage>(msgID, this);

		// Send a null-terminated message with a SendMessageDescriptor.
		// Note that the message is assembled directly in shared memory, and no
		// no copy from a user buffer is needed.
		MCSB::SendMessageDescriptor smd = client.GetSendMessageDescriptor(80);
		if (!smd.IsValid()) return;
		// smd now refers to a buffer of at least 80 bytes
		char* str = (char*)smd.Buf();
		sprintf(str, "Hello, world from %u!", msgID);
		uint32_t len = strlen(str)+1; // with null termination
		client.SendMessage(msgID, smd, len);
		//smd.SendMessage(msgID, len); // alternate/equivalent send with smd
	}

	~DescriptorHello(void)
	{
		// For completeness, we also de-register the callback upon destruction.
		client.DeregisterForMsgID<DescriptorHello,&DescriptorHello::HandleMessage>(msgID, this);
	}

  protected:
	MCSB::Client& client;
	uint32_t msgID;
	unsigned exitCount;
	unsigned msgsRcvd;
};

#endif
