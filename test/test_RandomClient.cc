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

#include <cstdlib>

#include "ClientTester.h"
#include "RandomClient.h"

class RandomClientTester : public MCSB::ClientTester {
  public:
	RandomClientTester(int argc, char* const argv[]);
   ~RandomClientTester(void) {}
	int RunClient(void);
  protected:
	int numMessages;
	int fillData;
	int contiguous;
};

//-----------------------------------------------------------------------------
RandomClientTester::RandomClientTester(int argc, char* const argv[])
//-----------------------------------------------------------------------------
:	MCSB::ClientTester(argc,argv), numMessages(100), fillData(1), contiguous(1)
{
	argc -= optind;
	argv += optind;

	if (argc>1) {
		numMessages = atoi(argv[1]);
	}
	if (argc>2) {
		fillData = !!atoi(argv[2]);
	}
	if (argc>3) {
		contiguous = !!atoi(argv[3]);
	}
	dbprintf(kInfo, "numMessages %d\n", numMessages);
	dbprintf(kInfo, "fillData %d\n", fillData);
	dbprintf(kInfo, "contiguous %d\n", contiguous);
}

//-----------------------------------------------------------------------------
int RandomClientTester::RunClient(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kInfo, "client[%d] starting\n", id);

	MCSB::Client client(opts);
	MCSB::RandomClient rc(&client, numMessages, id, fillData, contiguous);

	while (!rc.Done()) {
		if (client.Poll(.1)<0)
			usleep(10000);
	}

	return !!rc.TotalErrors();
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	RandomClientTester tester(argc,argv);
	return tester.Test();
}
