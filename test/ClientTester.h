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

#ifndef MCSB_ClientTester_h
#define MCSB_ClientTester_h
#pragma once

#include "MCSB/dbprinter.h"
#include "MCSB/TestingClientOptions.h"

namespace MCSB {

class ClientTester : public dbprinter {
  public:
	ClientTester(int argc, char* const argv[]);
	virtual ~ClientTester(void);
	virtual int Test(void);
	virtual int RunClient(void);
	virtual int RunManager(void);
	virtual int WaitForClients(void);
  protected:
	MCSB::TestingClientOptions opts;
	int id;
	int numClients;
};

} // namespace MCSB

#endif
