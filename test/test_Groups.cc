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
#include "MCSB/ClientWatcher.h"
#include "MCSB/Manager.h"
#include "ClientTester.h"

#include <cstdlib>
#include <deque>

class GroupHelper {
  public:
	GroupHelper(const MCSB::ClientOptions& clientOptions, int id);
   ~GroupHelper(void) {}
	int ID(void) const { return id; }
	MCSB::Client& Client(void) { return client; }
	void HandleMessage(uint32_t msgID, const void* msg, uint32_t len);
	void HandleRegistration(bool reg, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count);
	int messagesReceived;
	int registrationsReceived;
  private:
	MCSB::Client client;
	MCSB::ClientWatcher watcher;
	int id;
	void ConnectionEventHandler(int which) {
		fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
		ev::default_loop loop;
		loop.run(EVRUN_NOWAIT);
	}
};

//-----------------------------------------------------------------------------
GroupHelper::GroupHelper(const MCSB::ClientOptions& clientOptions, int id_)
//-----------------------------------------------------------------------------
:	messagesReceived(0), registrationsReceived(0),
	client(clientOptions,0), watcher(client,0,.1),
	id(id_)
{
	client.SetConnectionEventHandler<GroupHelper,
		&GroupHelper::ConnectionEventHandler>(this);
}

//-----------------------------------------------------------------------------
void GroupHelper::HandleMessage(uint32_t mid, const void* ptr, uint32_t len)
//-----------------------------------------------------------------------------
{
	printf("GroupHelper::HandleMessage[%d] (mid %u)\n", id, mid);
	messagesReceived++;
}

//-----------------------------------------------------------------------------
void GroupHelper::HandleRegistration(bool reg, int16_t clientID, int16_t groupID,
	const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	printf("HandleRegistration %s (cid %d/gid %d) [ %u", reg?"reg":"dereg",
		clientID, groupID, msgIDs[0]);
	for (unsigned i=1; i<count; i++) {
		printf(", %u", msgIDs[i]);
	}
	printf(" ]\n");
	registrationsReceived += count;
}

//-----------------------------------------------------------------------------
int test1(MCSB::ClientOptions &opts, int numClients)
//-----------------------------------------------------------------------------
{
	fprintf(stderr, "beginning test\n");

	std::deque<GroupHelper*> helpers;
	ev::default_loop loop;

	for (int i=0; i<numClients; i++) {
		MCSB::ClientOptions copts = opts;
		char str[80];
		sprintf(str,"-id%d", i);
		copts.clientName += str;
		GroupHelper* helper = new GroupHelper(copts, i);
		helpers.push_back(helper); 
		helpers[i]->Client().Poll(0.1);
	}

	for (int i=0; i<numClients; i++) {
		helpers[i]->Client().Poll(0.1);
		loop.run(EVRUN_NOWAIT);
	}

	// register everyone for a message
	uint32_t mid = 10;
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		mid = 10;
		helpers[i]->Client().RegisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		mid = 11;
		helpers[i]->Client().RegisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// tell the manager we want registration messages
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		helpers[i]->Client().SetRegistrationHandler<GroupHelper,
			&GroupHelper::HandleRegistration>(helpers[i]);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// send everyone a message
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		helpers[i]->Client().SendMessage(10,0,0);
		helpers[i]->Client().SendMessage(11,0,0);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// give Poll a chance
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// verify everyone got messages
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		GroupHelper* client = helpers[i];
		fprintf(stderr, "client[%d] received %d messages\n", client->ID(), client->messagesReceived);
		fprintf(stderr, "client[%d] received %d registrations\n", client->ID(), client->registrationsReceived);
		if ( client->messagesReceived != numClients*2 ) return -1;
		if ( client->registrationsReceived != numClients*2 ) return -1;
		client->messagesReceived = 0;
		client->registrationsReceived = 0;
	}

	// deregister everyone for the messages
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		mid = 10;
		helpers[i]->Client().DeregisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		mid = 11;
		helpers[i]->Client().DeregisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// change the GroupIDs 
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		helpers[i]->Client().RequestGroupID("test_Groups");
		assert(helpers[i]->Client().GroupID()>0);
	}

	// register everyone for a message
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		mid = 10;
		helpers[i]->Client().RegisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		mid = 11;
		helpers[i]->Client().RegisterForMsgID<GroupHelper,
			&GroupHelper::HandleMessage>(mid,helpers[i]);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// send everyone a message
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		helpers[i]->Client().SendMessage(10,0,0);
		helpers[i]->Client().SendMessage(11,0,0);
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}
	
	// give Poll a chance
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
	}

	// verify nobody got messages
	for (int i=0; i<numClients; i++) {
		loop.run(EVRUN_NOWAIT);
		helpers[i]->Client().Poll(0.01);
		GroupHelper* client = helpers[i];
		fprintf(stderr, "client[%d] received %d messages\n", client->ID(), client->messagesReceived);
		fprintf(stderr, "client[%d] received %d registrations\n", client->ID(), client->registrationsReceived);
		if ( client->messagesReceived != 0 ) return -1;
		if ( client->registrationsReceived != numClients*4 ) return -1;
	}

	// disconnect
	while (helpers.size()) {
		GroupHelper* helper = helpers.back();
		helpers.pop_back();
		helper->Client().Close();
		delete helper;
		loop.run(EVRUN_NOWAIT);
	}

	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::TestingClientOptions opts(argc,argv);
	int numClients = 4;
	// parse the args for this program
	argc -= optind;
	argv += optind;
	if (argc) {
		numClients = atoi(argv[0]);
	}
	fprintf(stderr, "numClients %u\n", numClients);

	ev::default_loop loop;
	MCSB::ManagerParams mparms(opts.ManagerArgc(), opts.ManagerArgv());
	MCSB::Manager manager(mparms, loop);
	loop.run(EVRUN_NOWAIT);

	int result = test1(opts, numClients);
	if (!result)
		fprintf(stderr,"=== PASS ===\n");
	else
		printf("- result is %d\n", result);
	return result;
}
