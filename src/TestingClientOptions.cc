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

#include "MCSB/TestingClientOptions.h"

#include <cstdlib>
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
TestingClientOptions::TestingClientOptions(const char* argv0_, const char* usage)
//-----------------------------------------------------------------------------
:	ClientOptions(argv0_,usage), argv0(argv0_), mgrSockOpt("-c")
{
	// use different default ctrl socket and shmem names
	ctrlSockName = SubstituteUsername("/tmp/mcsb-%U-test.sock");
	mgrSockOpt += ctrlSockName;
	managerOpts.push_back("ManagerOpts");
	managerOpts.push_back(mgrSockOpt.c_str());
	otherOpts.push_back(argv0);
}

//-----------------------------------------------------------------------------
TestingClientOptions::TestingClientOptions(int argc, char* const argv[], const char* usage)
//-----------------------------------------------------------------------------
:	ClientOptions(argv[0],usage), argv0(argv[0]), mgrSockOpt("-fFc")
{
	// use different default ctrl socket and shmem names
	ctrlSockName = SubstituteUsername("/tmp/mcsb-%U-test.sock");
	mgrSockOpt += ctrlSockName;
	managerOpts.push_back("ManagerOpts");
	managerOpts.push_back(mgrSockOpt.c_str());
	otherOpts.push_back(argv0);

	int result = Parse(argc,argv);
	if (result<0) {
		throw std::runtime_error("Error in ClientOptions::Parse()");
	} else if (result>0) {
		exit(0);
	}
	if (verbosity>=kInfo)
		Print();
}

//-----------------------------------------------------------------------------
int TestingClientOptions::Parse(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	const char* xtraOpts = "m:o:";

	return ClientOptions::Parse(argc,argv,xtraOpts,&XtraOptCB,this);
}

//-----------------------------------------------------------------------------
int TestingClientOptions::XtraOptCB(int opt, const char* optarg, void* user)
//-----------------------------------------------------------------------------
{
	TestingClientOptions* This = (TestingClientOptions*)user;
	return This->HandleExtraOpt(opt,optarg);
}

//-----------------------------------------------------------------------------
int TestingClientOptions::HandleExtraOpt(int opt, const char* optarg)
//-----------------------------------------------------------------------------
{
	switch (opt) {
		case 'm':
			managerOpts.push_back(optarg);
			break;
		case 'o':
			otherOpts.push_back(optarg);
			break;
		case 'h':
		case '?':
			PrintUsage();
			return 1;
		default:
			PrintUsage();
			return -1;
	}
		
	return 0;
}

//-----------------------------------------------------------------------------
void TestingClientOptions::PrintUsage(FILE* f)
//-----------------------------------------------------------------------------
{
	ClientOptions::PrintUsage(argv0,f);
	fprintf(f, "  -m str    add option for manager (pass -m-h for MCSBManager help)\n");
	fprintf(f, "  -o str    add option for 'other'\n");
}

//-----------------------------------------------------------------------------
void TestingClientOptions::Print(const char* prefix, FILE* f) const
//-----------------------------------------------------------------------------
{
	ClientOptions::Print(prefix,f);
	PrintOptList(prefix,f,"managerOpts",managerOpts);
	PrintOptList(prefix,f,"otherOpts",otherOpts);
}
	
//-----------------------------------------------------------------------------
void TestingClientOptions::PrintOptList(const char* prefix, FILE* f,
	const char* opt, const std::vector<const char*>& list) const
//-----------------------------------------------------------------------------
{
	// print the otherOpts as a list
	fprintf(f, "%s%s: [", prefix, opt);
	for (size_t i=0; i<list.size(); i++) {
		if (i)
			fprintf(f, ", ");
		fprintf(f, "\"%s\"", list[i]);
	}
	fprintf(f, "]\n");
}

} // namespace MCSB
