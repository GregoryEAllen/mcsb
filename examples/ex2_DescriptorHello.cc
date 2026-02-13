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

// This example also repeatedly sends a "Hello" messages to itself, still using
// the callback-based receive interface.

// However, this example uses message descriptors to send and receive a message.

// Message descriptors are the elemental method for sending and receiving
// messages through an MCSB client. Other methods for sending and receiving
// messages are built on top of message descriptors.

// Use of the message descriptor API allows advanced features, including zero-copy
// sending and receiving of messages, out of order retirement of receive messages,
// and multiple outstanding send buffers.

// Recall that an MCSB Manager process must be running for this example.

// Follow the code in DescriptorHello.h
#include "DescriptorHello.h"

// the remainder is the same as an earlier example
int main(int argc, char* const argv[])
{
	MCSB::Client client(argc,argv);
	uint32_t messageID = 12345;
	DescriptorHello descHello(client, messageID);
	client.Run();
	return 0;
}
