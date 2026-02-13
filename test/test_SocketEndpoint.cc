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

#include "MCSB/SocketEndpoint.h"
#include "MCSB/ShmDefs.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

//-----------------------------------------------------------------------------
int test_child(int fd)
//-----------------------------------------------------------------------------
{
	try {
		MCSB::SocketEndpoint client( fd );
	
		uint32_t blocks[4] = { 1, 2, 3, 4 };
		client.SendBlockIDs(blocks,4);
		client.SendSlabIDs(blocks,4);

		MCSB::BlockInfo info[4];
		client.SendBlocksAndInfo(blocks,info,4);

		for (int len=0; len<5; len++) {
			char str[10];
			memset(str,'X',len);
			str[len] = 0;
			client.SendCtrlString(0, str);
		}

		int16_t clientID = 1;
		int16_t groupID = 2;
		
		client.SendDropReport(0, 0);
		client.SendDropReportAck();
		client.SendNumSlabs(0, 0);
		client.SendSequenceToken(0);
		client.SendClientID(clientID);
		client.SendGroupID(groupID);
		client.SendClientPID(0);
		client.SendRegistrationsWanted();
		unsigned numIDs = 500;
		uint32_t ids[numIDs];
		for (unsigned i=0; i<numIDs; i++) {
			ids[i] = i;
		}
		client.SendRegistration(MCSB::kRegType_RegisterList,clientID,groupID,ids,numIDs);
		client.SendRegistration(MCSB::kRegType_DeregisterList,clientID,groupID,ids,numIDs);
		client.SendRegistration(MCSB::kRegType_RegisterAllMsgs,clientID,groupID,ids,numIDs);

		client.SendBlockIDs(ids,numIDs);
		client.SendSlabIDs(ids,numIDs);
		client.SendManagerEcho();
		client.SendProdNiceLevel(1);

		usleep(100000);

		unsigned sendSize = 0;
		while (1) {
			unsigned newSendSize = client.GetSendSockBufSize();
			if (sendSize>=newSendSize) break;
			sendSize = newSendSize;
			fprintf(stderr,"- sendSize %u\n", sendSize);
			try {
				client.SetSendSockBufSize(sendSize*2);
			} catch (std::runtime_error err) {
				fprintf(stderr,"#-- %s\n", err.what());
			}
		}

		unsigned recvSize = 0;
		while (1) {
			unsigned newRecvSize = client.GetRecvSockBufSize();
			if (recvSize>=newRecvSize) break;
			recvSize = newRecvSize;
			fprintf(stderr,"- recvSize %u\n", recvSize);
			try {
				client.SetRecvSockBufSize(recvSize*2);
			} catch (std::runtime_error err) {
				fprintf(stderr,"#-- %s\n", err.what());
			}
		}

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
int main()
//-----------------------------------------------------------------------------
{
	int fd[2];
	int res = socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
	if (res<0) {
		perror("socketpair error");
		return -1;
	}

	pid_t pid = fork();
	
	if (!pid) { // I am the child
		close(fd[0]);
		return test_child(fd[1]);
	}
	
	close(fd[1]);

	MCSB::SocketEndpoint client( fd[0] );
	int status;

	// poll the socket until the peer disconnects (and exception)
	while (true) {
		try {
			client.Poll(.1);
		} catch (std::runtime_error err) {
			fprintf(stderr,"- poll threw: %s\n", err.what());
			break;
		}
	}
	
	res = wait(&status);
	if (res<0) {
		perror("wait error");
		return -1;
	}

	int childResult = -1;
	if ( WIFEXITED(status) )
		childResult = WEXITSTATUS(status);
	
	if (!childResult)
		fprintf(stderr,"PASS\n");
	
	return childResult;
}
