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

#include "MCSB/Client.h"
#include "MCSB/VariantClientOptions.h"
#include "MCSB/uptimer.h"
#include <stdlib.h>
#include <stdexcept>

// blast (empty) messages as fast as possible
class MsgBlaster : public MCSB::uptimer {
  public:
	MsgBlaster(MCSB::BaseClient& client, unsigned msgID, unsigned msgSize, bool zeroFill);
	~MsgBlaster(void) { client.Flush(1); }
	int Run(unsigned numMsgs);
	int RunForInterval(double interval);

  protected:
	MCSB::BaseClient& client;
	unsigned msgID;
	unsigned msgSize;
	bool zeroFill;
	bool contiguous;
	unsigned msgsSent;
	
	double lastReportTime;
	void PrintStatus(void);
	void PrintResults(void);
	enum { kDefaultSendMessagesCount = 10000 };
	void SendMessages(unsigned count=kDefaultSendMessagesCount);
};

//-----------------------------------------------------------------------------
MsgBlaster::MsgBlaster(MCSB::BaseClient& client_, unsigned msgID_,
	unsigned msgSize_, bool zeroFill_)
//-----------------------------------------------------------------------------
: client(client_), msgID(msgID_), msgSize(msgSize_), zeroFill(zeroFill_),
	contiguous(0), msgsSent(0), lastReportTime(0)
{
	if (msgSize>client.MaxSendMessageSize()) {
		fprintf(stderr, "msgSize > max (%u)\n", client.MaxSendMessageSize());
	}
}

//-----------------------------------------------------------------------------
void MsgBlaster::SendMessages(unsigned count)
//-----------------------------------------------------------------------------
{
	for (unsigned i=0; i<count; i++) {
		MCSB::SendMessageDescriptor smd =
			client.GetSendMessageDescriptor(msgSize,contiguous);
		if (zeroFill) {
			smd.Fill();
		}
		if (smd.SendMessage(msgID,msgSize)>0) {
			msgsSent++;
		}
	}
}

//-----------------------------------------------------------------------------
int MsgBlaster::Run(unsigned numMsgs)
//-----------------------------------------------------------------------------
{
	while (1) {
		unsigned count = numMsgs-msgsSent;
		if (count>kDefaultSendMessagesCount)
			count = kDefaultSendMessagesCount;
		SendMessages(count);
		if (msgsSent>=numMsgs) break;
		PrintStatus();
	}
	PrintResults();
	return 0;
}

//-----------------------------------------------------------------------------
int MsgBlaster::RunForInterval(double interval)
//-----------------------------------------------------------------------------
{
	while(1) {
		SendMessages();
		if (uptime()>=interval) break;
		PrintStatus();
	}
	PrintResults();
	return 0;
}

//-----------------------------------------------------------------------------
void MsgBlaster::PrintStatus(void)
//-----------------------------------------------------------------------------
{
	if (uptime()-lastReportTime<1)
		return;
	lastReportTime += 1;
	char upt[80];
	uptime(upt,80);
	fprintf(stderr, "# uptime: %s, msgsSent: %u\n", upt, msgsSent);
}

//-----------------------------------------------------------------------------
void MsgBlaster::PrintResults(void)
//-----------------------------------------------------------------------------
{
	double elapsed = uptime();
	printf("---\n");
	printf("msgID: %u\n", msgID);
	printf("msgSize: %u\n", msgSize);
	printf("msgsSent: %u\n", msgsSent);
	printf("startTime: %f\n", InitTime());
	printf("elapsedTime: %f\n", elapsed);
	printf("msgRate: %1.4g\n", msgsSent/elapsed);
	printf("byteRate: %1.4g\n", msgsSent/elapsed*msgSize);
	printf("zeroFill: %d\n", (int)zeroFill);
	printf("...\n");
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	libvariant::ArgParse args(*argv);
	args.SetDescription("Blast empty messages as fast as possible.");

	MCSB::VariantClientOptionsHelper(args,0)
		.DefaultMinConsumerBytes(0);

	unsigned msgID = 10000;
	args.AddArgument("msgID", "Message ID to where messages are blasted")
		.Type(libvariant::ARGTYPE_UINT).MinArgs(0).Default(msgID);
	unsigned msgSize = 65536;
	args.AddOption("size", 's', "size", "Size of each message sent")
		.Type(libvariant::ARGTYPE_UINT).Default(msgSize);
	unsigned numMsgs = 100000;
	args.AddOption("count", 'c', "count", "Send number of messages, then exit")
		.Type(libvariant::ARGTYPE_UINT).Default(numMsgs);
	args.AddOption("time", 't', "time", "Send messages for time, then exit")
		.Type(libvariant::ARGTYPE_FLOAT);
	bool zeroFill = false;
	args.AddOption("zero", 'z', "zero", "Zero-fill messages before sending")
		.Action(libvariant::ARGACTION_STORE_TRUE).Type(libvariant::ARGTYPE_BOOL).Default(zeroFill);
	args.AddOption("verbosity", 'v', "", "Increase verbosity")
		.Type(libvariant::ARGTYPE_UINT).Action(libvariant::ARGACTION_COUNT);

	libvariant::Variant var = args.Parse(argc, argv);
	MCSB::ClientOptions opts = MCSB::VariantClientOptions(var);

	unsigned verbosity = libvariant::variant_cast<unsigned>(var["verbosity"]);
	if (verbosity>1) {
		std::string s = libvariant::SerializeYAML(var);
		fprintf(stderr,"%s\n", s.c_str());
	}
	opts.verbosity += verbosity;

	msgID = libvariant::variant_cast<unsigned>(var["msgID"]);
	msgSize = libvariant::variant_cast<unsigned>(var["size"]);
	numMsgs = libvariant::variant_cast<unsigned>(var["count"]);
	zeroFill = libvariant::variant_cast<bool>(var["zero"]);
	double exitTime = -1;
	if (var.Contains("time")) {
		exitTime = libvariant::variant_cast<double>(var["time"]);
	}

	MCSB::BaseClient client(opts);
	// wait until the client is ready to send messages
	while (!client.MaxSendMessageSize()) {
		if (client.Poll()<0)
			sleep(1);
	}

	MsgBlaster blaster(client,msgID,msgSize,zeroFill);
	if (exitTime>0)
		return blaster.RunForInterval(exitTime);
	return blaster.Run(numMsgs);
}
