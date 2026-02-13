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
#include "MCSB/uptimer.h"
#include "MCSB/dbprinter.h"
#include <stdlib.h>
#include <ev++.h>

class MCSBSource : MCSB::uptimer, MCSB::dbprinter {
  public:
	MCSBSource(MCSB::BaseClient& client_, int msgID_, unsigned msgSize_, float rateBps)
	:	client(client_), msgID(msgID_), msgSize(msgSize_),
		targetRate(rateBps), msgsSent(0), bytesSent(0), lastRateErr(0), Iout(0)
	{
		Verbosity(client.Verbosity());
		ev::loop_ref loop = ev::get_default_loop();
		stdinWatcher.loop = loop;
		reportTimer.loop = loop;
		sendTimer.loop = loop;
		stdinWatcher.set<MCSBSource, &MCSBSource::HandleStdin>(this);
		stdinWatcher.start(STDIN_FILENO, ev::READ);
		reportTimer.set<MCSBSource, &MCSBSource::HandleReportTimer>(this);
		reportPeriod = 1.;
		reportTimer.set(reportPeriod,reportPeriod);
		sendTimer.set<MCSBSource, &MCSBSource::HandleSendTimer>(this);
		sendPeriod = .01;
		sendTimer.set(sendPeriod,sendPeriod);
		bytesPerLoop = targetRate*sendPeriod;
		reportTimer.start();
		sendTimer.start();
		lastTime = CurrentTime();
	}
	enum { kMaxRate = 100000 };
  protected:
	MCSB::BaseClient& client;
	uint32_t msgID;
	unsigned msgSize;
	float targetRate;
	double lastTime;
	uint64_t msgsSent;
	uint64_t bytesSent;
	uint64_t lastMsgsSent;
	uint64_t lastBytesSent;
	double reportPeriod;
	double sendPeriod;
	double lastPeriod;
	double bytesPerLoop;
	double lastRateErr;
	double Iout;

	ev::io stdinWatcher;
	void HandleStdin(void);
	ev::timer sendTimer;
	void HandleSendTimer(void);
	ev::timer reportTimer;
	void HandleReportTimer(void) {
		double elapsedTime = uptime();
		char str[80];
		uptime(str,sizeof(str));
		int lvl = kNotice;
		dbprintf(lvl, "---\n");
		dbprintf(lvl, "uptime: %s\n", str);
		dbprintf(lvl, "msgsSent: %lu\n", (unsigned long)msgsSent);
		dbprintf(lvl, "bytesSent: %lu\n", (unsigned long)bytesSent);
		dbprintf(lvl, "targetRate: %g\n", targetRate);
		float msgRate = (msgsSent-lastMsgsSent)/reportPeriod;
		float byteRate = (bytesSent-lastBytesSent)/reportPeriod;
		dbprintf(lvl, "msgRate: %g\n", msgRate);
		dbprintf(lvl, "byteRate: %g\n", byteRate);
		dbprintf(lvl, "bytesPerLoop: %g\n", bytesPerLoop);
		dbprintf(lvl, "msgRateTot: %g\n", msgsSent/elapsedTime);
		dbprintf(lvl, "byteRateTot: %g\n", bytesSent/elapsedTime);
		dbprintf(lvl, "...\n");
		lastBytesSent = bytesSent;
		lastMsgsSent = msgsSent;
	}
};

//-----------------------------------------------------------------------------
void MCSBSource::HandleSendTimer(void)
//-----------------------------------------------------------------------------
{
	if (!client.Connected()) {
		if (!client.Connect())
			sleep(1);
	}

	double currTime = CurrentTime();
	double deltaTime = currTime-lastTime;
	lastTime = currTime;

	long loopBytesSent = 0;
	while (loopBytesSent<bytesPerLoop) {
		bool contiguous=0;
		MCSB::SendMessageDescriptor smd;
		smd = client.GetSendMessageDescriptor(msgSize,contiguous);
		if (!smd.Valid()) {
			return;
		}
		if (smd.SendMessage(msgID,msgSize)>0) {
			msgsSent++;
			bytesSent += msgSize;
			loopBytesSent += msgSize;
		}
	}

	// this is a PID controller that attempts to match targetRate
	double measRate = loopBytesSent/deltaTime;
	double rateError = measRate-targetRate;
	double rateToBytesScale = -sendPeriod;
	double loopBytesErr = rateError*rateToBytesScale;
	double deltaLoopBytesErr = (lastRateErr-rateError)*rateToBytesScale;
	lastRateErr = rateError;
	
	double Ku = .5; // params from measurements
	double Pu = .05;
	double Kp = 0.6 * Ku; // Ziegler–Nichols method
	double Ki = 2*Kp/Pu;
	double Kd = Kp*Pu/8;
	
	// compute the PID terms
	double Pout = Kp * loopBytesErr;
	Iout += Ki * loopBytesErr * deltaTime;
	double Dout = Kd * deltaLoopBytesErr / deltaTime;
	double newBytesPerLoop = Pout + Iout + Dout;

	double maxBytesPerLoop = 1e9;
	if (newBytesPerLoop>maxBytesPerLoop) newBytesPerLoop = maxBytesPerLoop;
	if (newBytesPerLoop<0) newBytesPerLoop = 0;
	bytesPerLoop = newBytesPerLoop;

//	dbprintf(kDebug, "%g %g %g %g %g %g\n", targetRate, measRate, newBytesPerLoop, Pout, Iout, Dout);
}

//-----------------------------------------------------------------------------
void MCSBSource::HandleStdin(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug, "%s\n", __PRETTY_FUNCTION__);
	ev::loop_ref loop = ev::get_default_loop();
	char *cptr;
	char buffer[256];
	
	cptr = fgets(buffer, 256, stdin);
	
	// ctrl-D makes us quit
	if (!cptr) {
		loop.break_loop(ev::ALL);
		return;
	}

	// a number sets the target data rate
	float newTargetRate;
	if (sscanf(cptr,"%f",&newTargetRate)) {
		if ((newTargetRate>kMaxRate)||(newTargetRate<1))
			printf("invalid rate, must be between 1 and %d MB/s\n", kMaxRate);
		else
			targetRate = newTargetRate*1e6;
		return;
	}

	// otherwise, some character command
	switch (cptr[0]) {
		case 'h':
			printf("=== interactive help ===\n");
			printf("  [0-9]+  set target data rate\n");
			printf("      q   quit\n\n");
			break;
		case 'q':
			loop.break_loop(ev::ALL);
			break;
		default:
			printf("  invalid input: '%c'\n", cptr[0]);
			break;
	}
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	std::string usage = "usage: %s [options] [msgID [msgSize [rate]]]\n";
	usage += "  send messages on msgID (default is process pid)\n";
	usage += "  optionally specify the message size (default 65536)\n";
	usage += "  and the nominal data rate in MB/s (default 40)";
	MCSB::ClientOptions opts(argc,argv,usage.c_str());

	// parse the args for this program
	argc -= optind;
	argv += optind;
	
	int msgID = -1;
	int msgSize = 65536;
	float rate = 40;
	if (argc) {
		msgID = atoi(argv[0]);
	}
	if (argc>1) {
		msgSize = atoi(argv[1]);
	}
	if (argc>2) {
		rate = atof(argv[2]);
		if (rate<=0) rate = 40;
		if (rate>MCSBSource::kMaxRate) rate = MCSBSource::kMaxRate;
	}
	
	if (msgID<0) msgID = getpid();

	MCSB::BaseClient client(opts);
	while (!client.MaxSendMessageSize()) {
		if (client.Poll()<0)
			sleep(1);
	}
	if (msgSize>(int)client.MaxSendMessageSize()) {
		fprintf(stderr, "msgSize > max (%u)\n", client.MaxSendMessageSize());
		return -1;
	}

	MCSBSource source(client,msgID,msgSize,rate*1e6);
	ev::default_loop loop;
	loop.run();
	
	return 0;
}
