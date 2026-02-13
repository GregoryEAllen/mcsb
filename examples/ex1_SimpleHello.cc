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

// This demonstrates a simple MCSB::Client example that repeatedly sends
// a "Hello" messages to itself.

// This example uses the "traditional" middleware interface.
// Messages are sent and received with a messageID, a pointer, and a length.
// This basic interface is reminiscent of read and write in the standard C
// library, which goes back to at least the 1960s.
//   ssize_t write(int fildes, const void *buf, size_t nbyte);

// Receipt of messages in this interface is callback-based (reactive,
// event-driven). Clients register to receive callbacks for messages with a
// specified messageID, and are called back when such messages arrive.

// This "traditional" interface has been in use in various middlewares
// and applications at ARL (and peer organizations) since at least the mid
// 1990s (when our group at ARL was first exposed to it).

// In all of these examples, an MCSB Manager process must be running
// (in another terminal) to which this client will connect.
// (Start an MCSB Manager if you haven't already).

// Assuming you can build MCSB with "make", then the examples are built with
// "make examples".

#include "MCSB/Client.h"

#include <cstdio>
#include <cstdint>
#include <unistd.h>

class SimpleHello {
  public:
	// This method will be called back upon receipt of a message.
	// This is the "traditional" function signature for an MCSB message callback.
	void HandleMessage(uint32_t msgID, const void* msg, uint32_t len)
	{
		printf("%s\n", (const char*)msg);
		printf("(control-C to exit)\n");
		// re-send the same message
		client.SendMessage(msgID,msg,len);
		// we sleep to slow down the messages
		// (but you would never do this in real application)
		usleep(100000);
	}

	// This constructor will install a message handler and send a message.
	// These could also have been done in main (but I prefer them here).
	SimpleHello(MCSB::Client& client_, uint32_t msgID_)
		: client(client_), msgID(msgID_)
	{
		// Here we tell the client we would like it to call the above method
		// upon receipt of a message with msgID.
		client.RegisterForMsgID<SimpleHello,&SimpleHello::HandleMessage>(msgID, this);
		// RegisterForMsgID<ClassType,&ClassType::Method>(uint32_t,ClassType*)
		// is a templated thunk, a fairly common pattern for C++ callbacks.

		// send a null-terminated message (to ourselves)
		char str[80];
		sprintf(str, "Hello, world from %u!", msgID);
		client.SendMessage(msgID, str, strlen(str)+1);
	}

	~SimpleHello(void)
	{
		// For completeness, we also de-register the callback upon destruction.
		client.DeregisterForMsgID<SimpleHello,&SimpleHello::HandleMessage>(msgID, this);
	}

  protected:
	MCSB::Client& client;
	uint32_t msgID;
};

// Create a client and a SimpleHello, and profit!
int main_1(int argc, char* const argv[])
{
	// We create a client directly from argc/argv
	MCSB::Client client(argc,argv);
	// MCSB makes it easy for all programs in a system to have the same
	// command-line arguments (and/or environment variables) for the various
	// MCSB client parameters.
	// The standard and default client options are in MCSB/ClientOptions.h
	// You get a working -h for help flag.

	// The instantiated client will attempt to connect to the MCSB Manager.
	
	// This is the messageID we will use to send/receive a message.
	uint32_t messageID = 12345;

	// Create an instance of SimpleHello, which will register
	// its own callback with the passed-in client.
	SimpleHello simpleHello(client, messageID);

	// In this example we give up our main loop.
	// The client will communicate with the manager and call the installed
	// callbacks on receipt of a message.
	client.Run();
	// However it's easy to have more control over your main loop.
	// See the next example, below.

	return 0;
}

// Introduce ClientOptions and a custom main loop.
int main_2(int argc, char* const argv[])
{
	// This time we create a ClientOptions object and pass it to the client.
	// This is more verbose, but more flexible.
	// For example, sometimes we may want to change defaults.
	MCSB::ClientOptions options(argv[0]); // argv[0] to get program name
	// options.verbosity += 1; // for example
	options.Parse(argc,argv);
	// MCSB/VariantClientOptions.h contains a more advanced argument parser
	// that uses libvariant::ArgParse (patterned after python's ArgParse).
	// VariantClientOptions are demonstrated in a different example.

	// This is just the same as above.
	MCSB::Client client(options);
	uint32_t messageID = 12345;
	SimpleHello simpleHello(client, messageID);
	
	// Here we control our own main loop (instead of calling client.Run()).
	while (true) {
		// Poll will communicate with the manager and then call
		// callbacks for any received messages.
		// If Poll returns<0, there was a problem connecting to the manager,
		// and we wait awhile before trying again.
		// It's common for a client process to repeatedly try connecting to
		// the manager until it succeeds, and only terminate because of an
		// external signal (like control-C).
		if (client.Poll()<0) {
			printf(">>>> Is MCSBManager running? <<<<\n");
			sleep(1);
		}
	}
	return 0;
}

int main(int argc, char* const argv[])
{
//	return main_1(argc,argv);
	return main_2(argc,argv);
}
