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
#include "MCSB/ClientWatcher.h"
#include "MCSB/uptimer.h"
#include "MCSB/dbprinter.h"
#include <stdlib.h>
#include <vector>

class MCSBSink : MCSB::uptimer, MCSB::dbprinter {
  public:
	MCSBSink(MCSB::Client& client_, uint32_t msgID, unsigned usec_=0)
	: client(client_), watcher(client), usec(usec_),
		echoMID(MCSB::kInvalidMessageID), ackMID(MCSB::kInvalidMessageID),
		exit(0), exitCount(0), exitTime(0),
		bytesRcvd(0), msgsRcvd(0), lastBytesRcvd(0), lastMsgsRcvd(0),
		segmentsDropped(0), bytesDropped(0)
	{
		Verbosity(client.Verbosity());
		client.SetDropReportHandler<MCSBSink, &MCSBSink::HandleDropReport>(this);
		client.RegisterForMsgID<MCSBSink, &MCSBSink::HandleMessage>(msgID, this);
		sigintWatcher.set<MCSBSink, &MCSBSink::HandleSigInt>(this);
		sigtermWatcher.set<MCSBSink, &MCSBSink::HandleSigInt>(this);
		sigintWatcher.start(SIGINT);
		sigtermWatcher.start(SIGTERM);
		reportTimer.loop = ev::get_default_loop();
		reportTimer.set<MCSBSink, &MCSBSink::HandleReportTimer>(this);
		reportPeriod = 1.;
		reportTimer.set(reportPeriod,reportPeriod);
		reportTimer.start();
	}

	void SetEchoMsgID(uint32_t mid) { echoMID = mid; }
	void SetAckMsgID(uint32_t mid) { ackMID = mid; }
	void SetExitCount(unsigned c) { exitCount = c; }
	void SetExitTime(double t) { exitTime = t; }
	
	void PrintFinalReport(void);

  protected:
	MCSB::Client& client;
	MCSB::ClientWatcher watcher;
	ev::timer reportTimer;
	ev::sig sigintWatcher;
	ev::sig sigtermWatcher;
	float reportPeriod;
	unsigned usec;
	uint32_t echoMID;
	uint32_t ackMID;
	bool exit;
	unsigned exitCount;
	double exitTime;
	std::vector<struct iovec> iov;

	uint64_t bytesRcvd;
	uint64_t msgsRcvd;
	uint64_t lastBytesRcvd;
	uint64_t lastMsgsRcvd;
	uint64_t segmentsDropped;
	uint64_t bytesDropped;

	void PrintIntervalReport(void);
	void PrintStats(FILE* f=stderr);

	void HandleMessage(const MCSB::RecvMessageDescriptor& desc) {
		dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
		msgsRcvd += 1;
		bytesRcvd += desc.TotalSize();
		if (usec>0)
			usleep(usec);
		if (ackMID!=MCSB::kInvalidMessageID) {
			unsigned sendSize = sizeof(uint64_t);
			if (sendSize>desc.Size())
				sendSize = desc.Size();
			client.SendMessage(ackMID, desc.Buf(), sendSize);
		}
		if (echoMID!=MCSB::kInvalidMessageID) {
			// get an iovec from the RecvMessageDescriptor 
			while(1) {
				int iovcnt = desc.GetIovec(&iov[0], iov.size());
				if (iovcnt>=0) break;
				iov.resize(-iovcnt);
			}
			// echo the message out on sendMID
			client.SendMessage(echoMID, &iov[0], iov.size());
		}
		CheckExit();
	}

	void HandleSigInt(void) {
		printf("\n");
		reportTimer.loop.break_loop();
	}

	void CheckExit(double elapsedTime=0) {
		if (exit)
			return;
		if (!elapsedTime)
			elapsedTime = uptime();
		exit |= exitCount>0 && msgsRcvd>=exitCount;
		exit |= exitTime>0 && uptime()>=exitTime;
		if (!exit)
			return;
		reportTimer.loop.break_loop();
	}

	void HandleDropReport(uint32_t segs, uint32_t bytes) {
		dbprintf(kNotice,"# DropReport: { segments: %u, bytes: %u }\n", segs, bytes);
		segmentsDropped += segs;
		bytesDropped += bytes;
	}

	void HandleReportTimer(void) {
		CheckExit();
		PrintIntervalReport();
	}
};

//-----------------------------------------------------------------------------
void MCSBSink::PrintStats(FILE* f)
//-----------------------------------------------------------------------------
{
	double elapsedTime = uptime();
	fprintf(f, "elapsedTime: %f\n", elapsedTime);
	fprintf(f, "msgsRcvd: %llu\n", (unsigned long long)msgsRcvd);
	fprintf(f, "bytesRcvd: %llu\n", (unsigned long long)bytesRcvd);
	fprintf(f, "segmentsDropped: %llu\n", (unsigned long long)segmentsDropped);
	fprintf(f, "bytesDropped: %llu\n", (unsigned long long)bytesDropped);
	fprintf(f, "totalMsgRate: %g\n", msgsRcvd/elapsedTime);
	fprintf(f, "totalByteRate: %g\n", bytesRcvd/elapsedTime);
}

//-----------------------------------------------------------------------------
void MCSBSink::PrintIntervalReport(void)
//-----------------------------------------------------------------------------
{
	if (Verbosity()<kInfo) return;
	fprintf(stderr, "---\n");
	PrintStats(stderr);
	float msgRate = (msgsRcvd-lastMsgsRcvd)/reportPeriod;
	float byteRate = (bytesRcvd-lastBytesRcvd)/reportPeriod;
	fprintf(stderr, "intervalMsgRate: %g\n", msgRate);
	fprintf(stderr, "intervalByteRate: %g\n", byteRate);
	fprintf(stderr, "...\n");
	lastBytesRcvd = bytesRcvd;
	lastMsgsRcvd = msgsRcvd;
}

//-----------------------------------------------------------------------------
void MCSBSink::PrintFinalReport(void)
//-----------------------------------------------------------------------------
{
	fprintf(stdout, "---\n");
	PrintStats(stdout);
	fprintf(stdout, "...\n");
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	libvariant::ArgParse args(*argv);
	args.SetDescription("Receive messages and print report.");

	MCSB::VariantClientOptionsHelper(args,0);

	uint32_t msgID = 10000;
	args.AddArgument("msgID", "Message ID on which messages are received")
		.Type(libvariant::ARGTYPE_UINT).MinArgs(0).Default(msgID);
	unsigned usec = 0;
	args.AddOption("usec", 'u', "usec", "Wait after each received message (call usleep)")
		.Type(libvariant::ARGTYPE_UINT).Default(usec);
	args.AddOption("echoMsgID", 'e', "echoMsgID", "Echo received messages on echoMsgID")
		.Type(libvariant::ARGTYPE_UINT);
	args.AddOption("ackMsgID", 'a', "ackMsgID", "Ack with a (nearly) empty msg on ackMsgID")
		.Type(libvariant::ARGTYPE_UINT);
	args.AddOption("verbosity", 'v', "", "Increase verbosity")
		.Type(libvariant::ARGTYPE_UINT).Action(libvariant::ARGACTION_COUNT);
	args.AddOption("count", 'c', "count", "Exit after receiving specified number of messages")
		.Type(libvariant::ARGTYPE_UINT);
	args.AddOption("time", 't', "time", "Exit after specified time")
		.Type(libvariant::ARGTYPE_FLOAT);

	libvariant::Variant var = args.Parse(argc, argv);
	MCSB::ClientOptions opts = MCSB::VariantClientOptions(var);

	msgID = libvariant::variant_cast<unsigned>(var["msgID"]);
	usec = libvariant::variant_cast<unsigned>(var["usec"]);

	uint32_t echoMsgID = MCSB::kInvalidMessageID;
	if (var.Contains("echoMsgID")) {
		echoMsgID = libvariant::variant_cast<unsigned>(var["echoMsgID"]);
	}
	uint32_t ackMsgID = MCSB::kInvalidMessageID;
	if (var.Contains("ackMsgID")) {
		ackMsgID = libvariant::variant_cast<unsigned>(var["ackMsgID"]);
	}
	unsigned exitCount = -1;
	if (var.Contains("count")) {
		exitCount = libvariant::variant_cast<unsigned>(var["count"]);
	}
	double exitTime = unsigned(-1);
	if (var.Contains("time")) {
		exitTime = libvariant::variant_cast<double>(var["time"]);
	}

	unsigned verbosity = libvariant::variant_cast<unsigned>(var["verbosity"]);
	if (verbosity>1) {
		std::string s = libvariant::SerializeYAML(var);
		fprintf(stderr,"%s\n", s.c_str());
	}
	opts.verbosity += verbosity;

	MCSB::Client client(opts);
	MCSBSink sink(client,msgID,usec);
	sink.SetEchoMsgID(echoMsgID);
	sink.SetAckMsgID(ackMsgID);
	sink.SetExitCount(exitCount);
	sink.SetExitTime(exitTime);

	ev::default_loop loop;
	loop.run();

	sink.PrintFinalReport();

	return 0;
}
