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

#include "MCSB/VariantClientOptions.h"
#include "MCSB/Client.h"

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::ClientOptions opts = MCSB::VariantClientOptions(argc,argv);

	MCSB::ArgParse argParse;
	MCSB::VariantClientOptionsHelper(argParse)
		.DefaultMinProducerBytes(MCSB::ClientOptions::kDefaultMinProducerBytes/2)
		.DefaultMinConsumerBytes(MCSB::ClientOptions::kDefaultMinConsumerBytes/3)
		.DefaultMinProducerSlabs(3)
		.DefaultMinConsumerSlabs(2)
		.DefaultVerbosity(2);

	MCSB::Variant var = argParse.Parse(argc, argv);
	opts = MCSB::VariantClientOptions(var);

	if (opts.verbosity)
		opts.Print();
	
	return 0;
}
