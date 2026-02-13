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

#include "MCSB/ClientOptions.h"
#include "MCSB/TestingClientOptions.h"
#include "MCSB/ManagerParams.h"

#include <stdexcept>
#include <unistd.h>

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	try {
		// will throw on error
		MCSB::TestingClientOptions tcopts(argc,argv);

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}

	MCSB::TestingClientOptions opts(argv[0]);
	
	int result = opts.Parse(argc,argv);
	if (result) return result<0;

	MCSB::ManagerParams mparams(opts.ManagerArgc(), opts.ManagerArgv());

	if (opts.verbosity>1)
		opts.Print();
	
	return 0;
}
