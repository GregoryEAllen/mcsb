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
#include <cfloat>
#include <cmath>
#include <deque>

class RunningStats {
  public:
	RunningStats(void): count(0), m1(0), m2(0), mn(DBL_MAX), mx(DBL_MIN) {}
	void AddSample(double v);
	uint64_t Count(void) const { return count; }
	double Mean(void) const { return m1; }
	double Variance(void) const { return m2/(count-1); }
	double StdDev(void) const { return sqrt(Variance()); }
	double Min(void) const { return mn; }
	double Max(void) const { return mx; }
  protected:
	uint64_t count;
	double m1;
	double m2;
	double mn;
	double mx;
};

class MCSBPing : MCSB::uptimer, MCSB::dbprinter {
  public:
	MCSBPing(MCSB::Client& client_, uint32_t oMsgID_, uint32_t iMsgID_, uint32_t msgSize_,
		double interval_, bool flood_)
	: client(client_), watcher(client), oMsgID(oMsgID_), iMsgID(iMsgID_),
		interval(interval_), flood(flood_), timeout(1), msgSize(msgSize_),
		zeroFill(0), exitCount(0), preload(0),
		bytesSent(0), msgsSent(0), bytesRcvd(0), msgsRcvd(0),
		segmentsDropped(0), bytesDropped(0)
	{
		Verbosity(client.Verbosity());
		client.SetDropReportHandler<MCSBPing, &MCSBPing::HandleDropReport>(this);
		client.RegisterForMsgID<MCSBPing, &MCSBPing::HandleMessage>(iMsgID, this);
		sigintWatcher.set<MCSBPing, &MCSBPing::HandleSigInt>(this);
		sigtermWatcher.set<MCSBPing, &MCSBPing::HandleSigInt>(this);
		sigintWatcher.start(SIGINT);
		sigtermWatcher.start(SIGTERM);
		intervalTimer.loop = ev::get_default_loop();
		intervalTimer.set<MCSBPing, &MCSBPing::HandleIntervalTimer>(this);
		if (flood)
			interval = 0.01;
		intervalTimer.set(0,interval);
		intervalTimer.start();
	}

	void SetZeroFill(bool f) { zeroFill = f; }
	void SetExitCount(unsigned c) { exitCount = c; }
	void SetPreload(unsigned n) { preload = n; }
	void PrintFinalReport(void);
	int ResultCode(void) { return !(msgsRcvd==msgsSent); }

  protected:
	MCSB::Client& client;
	MCSB::ClientWatcher watcher;
	ev::sig sigintWatcher;
	ev::sig sigtermWatcher;
	uint32_t oMsgID;
	uint32_t iMsgID;
	ev::timer intervalTimer;
	float interval;
	bool flood;
	double timeout;
	uint32_t msgSize;
	bool zeroFill;
	uint32_t exitCount;
	unsigned preload;

	uint64_t bytesSent;
	uint64_t msgsSent;
	uint64_t bytesRcvd;
	uint64_t msgsRcvd;
	uint64_t segmentsDropped;
	uint64_t bytesDropped;
	RunningStats stats;

	typedef std::deque< std::pair<uint64_t,double> > InFlightMsgQueue;
	InFlightMsgQueue msgQueue;
	
	void HandleMessage(const MCSB::RecvMessageDescriptor& desc);

	void HandleSigInt(void) {
		printf("\n");
		if (flood && msgsSent) 
			exitCount = msgsSent-1; // catch any outstanding messages
		else
			intervalTimer.loop.break_loop();
	}

	void HandleDropReport(uint32_t segs, uint32_t bytes) {
		dbprintf(kNotice,"# DropReport: { segments: %u, bytes: %u }\n", segs, bytes);
		segmentsDropped += segs;
		bytesDropped += bytes;
	}

	void HandleIntervalTimer(void);
	void SendMessage(void);
	void CheckTimeouts(void);
};



//-----------------------------------------------------------------------------
void MCSBPing::HandleMessage(const MCSB::RecvMessageDescriptor& desc)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);

	unsigned size = desc.TotalSize();
	uint32_t msgID = desc.MessageID();
	
	bytesRcvd += size;
	msgsRcvd++;
	
	if (size<sizeof(uint64_t)) {
		dbprintf(kInfo,"# Message too small, size: %u, msgID: %u\n", size, msgID);
		return;
	}
	if (msgQueue.empty()) {
		dbprintf(kInfo,"# Empty queue, but recv'd message of size: %u, msgID: %u\n",
			size, msgID);
		return;
	}
	uint64_t msgSeqNum = *((const uint64_t*)desc.Buf());

	// check head of queue
	double sendTime = 0;
	while (msgQueue.size()) {
		InFlightMsgQueue::value_type front = msgQueue.front();
		msgQueue.pop_front();
		if (front.first==msgSeqNum) {
			sendTime = front.second;
			break;
		}
		dbprintf(kInfo,"# skipped seqNum %llu\n", (unsigned long long)(front.first));
	}
	
	if (!sendTime) {
		// never found a msgSeqNum match in msgQueue
		dbprintf(kInfo,"# rcvd msgSeqNum %llu, not in msgQueue\n",
			(unsigned long long)(msgSeqNum));
		return;
	}
	
	double roundTripTime = uptime()-sendTime;
	stats.AddSample(roundTripTime);

	if (!flood) {
		dbprintf(kNotice,"- { size: %u, msgID: %u, seq: %llu, time: %3.3fe-3 }\n",
			size, msgID, (unsigned long long)msgSeqNum, roundTripTime*1000);
	} else {
		dbprintf(kNotice,"\b");
		// send again!
		intervalTimer.stop();
		interval = 0.01;
		intervalTimer.set(interval,interval);
		intervalTimer.start();
		SendMessage();
	}
	CheckTimeouts();
}

//-----------------------------------------------------------------------------
void MCSBPing::HandleIntervalTimer(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	if (!msgsSent && preload) {
		for (unsigned i=0; i<preload; i++)
			SendMessage();
	}
	SendMessage();
	CheckTimeouts();
}

//-----------------------------------------------------------------------------
void MCSBPing::CheckTimeouts(void)
//-----------------------------------------------------------------------------
{
	// check for timeouts, pop expired messages
	double currentTime = uptime();
	while (msgQueue.size()) {
		InFlightMsgQueue::value_type front = msgQueue.front();
		if (currentTime-front.second<timeout) {
			break;
		}
		msgQueue.pop_front();
		dbprintf(kInfo,"# timeout on seqNum %llu\n", (unsigned long long)(front.first));
	}

	// check exitCount condition met
	if (!msgQueue.size() && exitCount>0 && msgsSent>=exitCount)
		intervalTimer.loop.break_loop();
}

//-----------------------------------------------------------------------------
void MCSBPing::SendMessage(void)
//-----------------------------------------------------------------------------
{
	if (exitCount>0 && msgsSent>=exitCount) {
		// refuse to send more than exitCount
		return;
	}

	bool contiguous = 0;
	MCSB::SendMessageDescriptor smd =
		client.GetSendMessageDescriptor(msgSize,contiguous);
	if (!smd.Valid()) {
		return;
	}
	if (zeroFill) {
		smd.Fill();
	}
	assert(msgSize>=sizeof(uint64_t));
	uint64_t* ptr = (uint64_t*)smd.Buf();
	*ptr = msgsSent;
	if (smd.SendMessage(oMsgID,msgSize)>0) {
		double sendTime = uptime();
		msgQueue.push_back(std::make_pair(*ptr,sendTime));
		msgsSent++;
		bytesSent += msgSize;
		if (flood)
			dbprintf(kNotice,".");
	}
}

//-----------------------------------------------------------------------------
void MCSBPing::PrintFinalReport(void)
//-----------------------------------------------------------------------------
{
	FILE* f = stdout;
	if (flood)
		dbprintf(kNotice,"\n");
	fprintf(f, "---\n");
	double elapsedTime = uptime();
	fprintf(f, "elapsedTime: %f\n", elapsedTime);
	fprintf(f, "msgsSent: %llu\n", (unsigned long long)msgsSent);
	fprintf(f, "msgsRcvd: %llu\n", (unsigned long long)msgsRcvd);
	double pctMsgLoss = 100-msgsRcvd*100./msgsSent;
	fprintf(f, "pctMsgLoss: %0.1f\n",pctMsgLoss);
	fprintf(f, "bytesSent: %llu\n", (unsigned long long)bytesSent);
	fprintf(f, "bytesRcvd: %llu\n", (unsigned long long)bytesRcvd);
	fprintf(f, "segmentsDropped: %llu\n", (unsigned long long)segmentsDropped);
	fprintf(f, "bytesDropped: %llu\n", (unsigned long long)bytesDropped);
	fprintf(f, "zeroFill: %d\n", (int)zeroFill);
	fprintf(f, "preload: %u\n", preload);
	fprintf(f, "msgRate: %g\n", msgsSent/elapsedTime);
	fprintf(f, "byteRate: %g\n", bytesSent/elapsedTime);
	fprintf(f, "flightTimeStats:\n");
	fprintf(f, "  min: %1.3g\n", stats.Min());
	fprintf(f, "  avg: %1.3g\n", stats.Mean());
	fprintf(f, "  max: %1.3g\n", stats.Max());
	fprintf(f, "  stddev: %1.3g\n", stats.StdDev());
	fprintf(f, "...\n");
}

//-----------------------------------------------------------------------------
void RunningStats::AddSample(double v)
//-----------------------------------------------------------------------------
{
	count++;
	double delta = v - m1;
	m1 += delta/count;
	m2 += delta*delta*(count-1)/count;
	if (mn>v) mn = v;
	if (mx<v) mx = v;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	libvariant::ArgParse args(*argv);
	args.SetDescription("Send messages, await replies, and report results.");

	MCSB::VariantClientOptionsHelper(args,0);

	uint32_t msgID = 10000;
	args.AddArgument("msgID", "Message ID on which messages are sent")
		.Type(libvariant::ARGTYPE_UINT).MinArgs(0).Default(msgID);
	args.AddOption("recvMsgID", 'r', "recvMsgID", "Use this msgID for recv")
		.Type(libvariant::ARGTYPE_UINT);
	unsigned msgSize = 65536;
	args.AddOption("size", 's', "size", "Size of each message sent")
		.Type(libvariant::ARGTYPE_UINT).Minimum(sizeof(uint64_t)).Default(msgSize);
	float interval = 1;
	args.AddOption("interval", 'i', "interval", "Time interval between messages")
		.Type(libvariant::ARGTYPE_FLOAT).Minimum(0.1).Default(interval);
	bool zeroFill = false;
	args.AddOption("zero", 'z', "zero", "Zero-fill messages before sending")
		.Action(libvariant::ARGACTION_STORE_TRUE).Type(libvariant::ARGTYPE_BOOL).Default(zeroFill);
	bool flood = false;
	args.AddOption("flood", 'f', "flood", "Flood ping, sending messages as they come in")
		.Action(libvariant::ARGACTION_STORE_TRUE).Type(libvariant::ARGTYPE_BOOL).Default(flood);
	unsigned preload = 0;
	args.AddOption("preload", 'l', "preload", "Quickly send additional messages on startup")
		.Type(libvariant::ARGTYPE_UINT).Default(0);
	args.AddOption("count", 'c', "count", "Exit after sending and waiting for number of messages")
		.Type(libvariant::ARGTYPE_UINT);
	args.AddOption("verbosity", 'v', "", "Increase verbosity")
		.Type(libvariant::ARGTYPE_UINT).Action(libvariant::ARGACTION_COUNT);

	libvariant::Variant var = args.Parse(argc, argv);
	MCSB::ClientOptions opts = MCSB::VariantClientOptions(var);

	msgID = libvariant::variant_cast<unsigned>(var["msgID"]);
	msgSize = libvariant::variant_cast<unsigned>(var["size"]);
	interval = libvariant::variant_cast<float>(var["interval"]);
	zeroFill = libvariant::variant_cast<bool>(var["zero"]);
	flood = libvariant::variant_cast<bool>(var["flood"]);
	preload = libvariant::variant_cast<unsigned>(var["preload"]);

	uint32_t iMsgID = msgID;
	if (var.Contains("recvMsgID")) {
		iMsgID = libvariant::variant_cast<unsigned>(var["recvMsgID"]);
	}
	unsigned exitCount = 0;
	if (var.Contains("count")) {
		exitCount = libvariant::variant_cast<unsigned>(var["count"]);
	}

	unsigned verbosity = libvariant::variant_cast<unsigned>(var["verbosity"]);
	if (verbosity>1) {
		std::string s = libvariant::SerializeYAML(var);
		fprintf(stderr,"%s\n", s.c_str());
	}
	opts.verbosity += verbosity;

	MCSB::Client client(opts);
	client.Poll(0.01);
	MCSBPing ping(client,msgID,iMsgID,msgSize,interval,flood);
	ping.SetZeroFill(zeroFill);
	ping.SetExitCount(exitCount);
	ping.SetPreload(preload);
	
	ev::default_loop loop;
	loop.run();

	ping.PrintFinalReport();
	return ping.ResultCode();
}
