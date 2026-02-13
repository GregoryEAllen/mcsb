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
#include <stdlib.h>
#include <map>
#include <set>

class RegStateHolder {
  public:
	RegStateHolder(void) {}
	void HandleRegMsg(bool reg, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count);
	void HandleConnectionEvent(int which);
  protected:
	void AddMsgIDs(int16_t clientID, int16_t groupID, const uint32_t msgIDs[],
		unsigned count);
	void DelMsgIDs(int16_t clientID, int16_t groupID, const uint32_t msgIDs[],
		unsigned count);
	void PrintClientRegs(void);

	struct ClientRegs {
		int16_t clientID;
		int16_t groupID;
		std::set<uint32_t> msgIDs;
		ClientRegs(int16_t cid, int16_t gid): clientID(cid), groupID(gid) {}
	};
	
	
	typedef std::map<int16_t,ClientRegs> CidToRegsMap;
	CidToRegsMap cidToRegs;
};

//-----------------------------------------------------------------------------
void RegStateHolder::HandleRegMsg(bool reg, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	printf("---\n");
	printf("RegistrationMessage:\n");
	printf("  reg: %d\n", reg);
	printf("  clientID: %d\n", clientID);
	printf("  groupID: %d\n", groupID);
	if (count) {
		printf("  msgIDs: [%d", msgIDs[0]);
		for (unsigned i=1; i<count; i++)
			printf(",%d", msgIDs[i]);
		printf("]\n");
	}
	printf("  count: %d\n", count);
	printf("...\n");

	if (reg)
		AddMsgIDs(clientID,groupID,msgIDs,count);
	else
		DelMsgIDs(clientID,groupID,msgIDs,count);
	
	PrintClientRegs();
}

//-----------------------------------------------------------------------------
void RegStateHolder::HandleConnectionEvent(int which)
//-----------------------------------------------------------------------------
{
	if (which!=MCSB::kDisconnection) return;
	cidToRegs.clear();
	PrintClientRegs();
}

//-----------------------------------------------------------------------------
void RegStateHolder::AddMsgIDs(int16_t clientID, int16_t groupID,
	const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	// find the Reg
	CidToRegsMap::iterator it = cidToRegs.find(clientID);
	if (it==cidToRegs.end()) {
		// new clientID
		ClientRegs regs(clientID,groupID);
		cidToRegs.insert( std::make_pair(clientID,regs) );
		it = cidToRegs.find(clientID);
	}
	
	// check for groupID consistency
	int16_t oldGroupID = it->second.groupID;
	if (oldGroupID != groupID) {
		fprintf(stderr,"### clientID %d changed groupID from %d to %d!\n",
			clientID,oldGroupID,groupID);
		it->second.groupID = groupID;
	}
	
	// insert the msgIDs
	for (unsigned i=0; i<count; i++) {
		std::pair<std::set<uint32_t>::iterator, bool> p =
			it->second.msgIDs.insert(msgIDs[i]);
		if (!p.second) {
			fprintf(stderr,"### clientID %d doubly inserted msgID %d!\n",
				clientID,msgIDs[i]);
		}
	}
}

//-----------------------------------------------------------------------------
void RegStateHolder::DelMsgIDs(int16_t clientID, int16_t groupID,
	const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	// find the Reg
	CidToRegsMap::iterator it = cidToRegs.find(clientID);
	if (it==cidToRegs.end()) {
		fprintf(stderr,"### deregistration from unknown clientID %d, groupID %d!\n",
			clientID,groupID);
	}
	
	// check for groupID consistency
	int16_t oldGroupID = it->second.groupID;
	if (oldGroupID != groupID) {
		fprintf(stderr,"### clientID %d changed groupID from %d to %d!\n",
			clientID,oldGroupID,groupID);
	}

	// erase the msgIDs
	for (unsigned i=0; i<count; i++) {
		bool erased = it->second.msgIDs.erase(msgIDs[i]);
		if (!erased) {
			fprintf(stderr,"### clientID %d erased unknown msgID %d!\n",
				clientID,msgIDs[i]);
		}
	}

	// check whether clientID should be removed
	if (it->second.msgIDs.empty()) {
		fprintf(stderr,"- removing clientID %d\n",clientID);
		cidToRegs.erase(it);
	}
}

//-----------------------------------------------------------------------------
void RegStateHolder::PrintClientRegs(void)
//-----------------------------------------------------------------------------
{
	typedef std::multimap<int16_t,int16_t> G2cMap;
	G2cMap gidToCid;
	CidToRegsMap::const_iterator it = cidToRegs.begin();
	for (; it!=cidToRegs.end(); ++it) {
		gidToCid.insert( std::make_pair(it->second.groupID,it->first) );
	}
	printf("---\n");
	if (!cidToRegs.size()) {
		printf("GroupMembership: null\n");
		printf("Registrations: null\n");
		printf("...\n");
		return;
	}
	printf("GroupMembership: # groupID: [clientIDs in group]\n");
	G2cMap::const_iterator git = gidToCid.begin();
	while (git!=gidToCid.end()) {
		int16_t gid = git->first;
		std::pair<G2cMap::const_iterator,G2cMap::const_iterator> pit;
		pit = gidToCid.equal_range(gid);
		git=pit.first;
		printf("  %d: [%d",gid,pit.first->second);
		for (++git; git!=pit.second; ++git) {
			printf(",%d",git->second);
		}
		printf("]\n");
	}
	printf("Registrations: # clientID: [registered msgIDs]\n");
	for (it=cidToRegs.begin(); it!=cidToRegs.end(); ++it) {
		std::set<uint32_t>::iterator mit = it->second.msgIDs.begin();
		std::set<uint32_t>::iterator eit = it->second.msgIDs.end();
		printf("  %d: [%d",it->first,*mit);
		for (++mit; mit!=eit; ++mit) {
			printf(",%d",*mit);
		}
		printf("]\n");
	}
	printf("...\n");
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::ClientOptions opts(argc,argv);
	MCSB::Client client(opts);
	RegStateHolder rsHolder;
	
	client.SetConnectionEventHandler<RegStateHolder,
		&RegStateHolder::HandleConnectionEvent>(&rsHolder);
	client.SetRegistrationHandler<RegStateHolder,
		&RegStateHolder::HandleRegMsg>(&rsHolder);
	
	client.Run();
	return 0;
}
