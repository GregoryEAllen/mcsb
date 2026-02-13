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

#ifndef MCSB_TestingClientOptions_h
#define MCSB_TestingClientOptions_h
#pragma once

#include "MCSB/ClientOptions.h"
#include <vector>
#include <string>

namespace MCSB {

// used in testing programs, can handle additional opts
// and for sending opts to a forked manager
class TestingClientOptions : public ClientOptions {
  public:
	TestingClientOptions(const char* argv0=0, const char* usage="");
	TestingClientOptions(int argc, char* const argv[], const char* usage="");
	virtual ~TestingClientOptions(void) {}
	int Parse(int argc, char* const argv[]);
	virtual void Print(const char* prefix="", FILE* f=stderr) const;

	int ManagerArgc(void) const { return managerOpts.size(); }
	char* const* ManagerArgv(void) { return (char* const*)&managerOpts[0]; }

	int OtherArgc(void) const { return otherOpts.size(); }
	char* const* OtherArgv(void) { return (char* const*)&otherOpts[0]; }

  protected:
	const char* argv0;
	static int XtraOptCB(int opt, const char* optarg, void* user);
	int HandleExtraOpt(int opt, const char* optarg);
	virtual void PrintUsage(FILE* f=stderr);

	std::vector<const char*> managerOpts;
	std::vector<const char*> otherOpts;
	std::string mgrSockOpt;

	void PrintOptList(const char* prefix, FILE* f,
		const char* opt, const std::vector<const char*>& list) const;
};

} // namespace MCSB

#endif
