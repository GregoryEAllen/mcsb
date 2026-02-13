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

// It's nice to have consistent set of command-line options across a set of
// programs in a system. For example, every MCSB client has the same set of
// parameters that a user may wish to adjust.

// libvariant [http://bitbucket.org/gallen/libvariant] contains ArgParse,
// a C++ argument parsing created in the style of Python's argparse.
// libvariant::ArgParse makes powerful command-line argument parsing
// fairly easy.

// A variant is an object that contains a value of some type.
// It can contain an integer, a float, a string, etc.
// It can also contain a list or map (dict) of variants.
// See libvariant/README.

// MCSB includes VariantClientOptions, which makes it easy to set parameters
// for an MCSB client. Additionally, it is easy to get and set parameters
// for your own programs that use an MCSB client.

// After following this code, run this program and use the help (-h) to
// try a few different options.

// Recall that an MCSB Manager process must be running for this example.

// Additional examples of programs that use MCSB with both libev and
// libvariant::ArgParse are available in the MCSB repository in apps/tools.

#include "MCSB/VariantClientOptions.h"
#include "MCSB/ClientWatcher.h"
#include <ev++.h>

// This is the same file used in ex2
#include "DescriptorHello.h"

// Use VariantClientOptions and libev
int main(int argc, char* const argv[])
{
	// Create a ClientOptions struct with VariantClientOptions
	// and no additional options or arguments.
//	MCSB::ClientOptions options = MCSB::VariantClientOptions(argc,argv);

	// What makes this powerful is the ability to mix a Client's options
	// with your own options that are specific to your program.

	// We create an ArgParse object and optionally set a description.
	libvariant::ArgParse args(*argv);
	args.SetDescription("An example program using MCSB, ArgParse, and libev.");

	// With VariantClientOptionsHelper, we tell the ArgParse object to
	// parse command-line options for an MCSB Client.
	MCSB::VariantClientOptionsHelper(args) // semi missing for next line
		// If we wish, default options can be changed (but still specified
		// on the command line). In this case, we are reducing (to zero)
		// the default minimum amount of space that this client will request
		// for sending messages. (This client will be given the minimum
		// amount possible, one slab). See VariantClientOptions.h.
		.DefaultMinProducerBytes(0); 

	// Now we add our own argument and option.
	// See the documentation for libvariant or Variant/ArgParse.h.
	uint32_t messageID = 12345;
	unsigned count = 0;
	args.AddArgument("messageID", "Message ID to use for sending hello.")
		.Type(libvariant::ARGTYPE_UINT).MinArgs(0).Default(messageID);
	args.AddOption("count", 'n', "count", "Send number of messages, then exit")
		.Type(libvariant::ARGTYPE_UINT).Default(count);

	// This line actually parses the arguments and puts the results
	// into a variant object.
	libvariant::Variant var = args.Parse(argc, argv);

	// We create a ClientOptions object from the resulting variant,
	// and then create an MCSB client.
	MCSB::ClientOptions options = MCSB::VariantClientOptions(var);
	MCSB::Client client(options);

	// We also pull our own settings out of the variant object.
	messageID = libvariant::variant_cast<uint32_t>(var["messageID"]);
	count = libvariant::variant_cast<unsigned>(var["count"]);

	// And then create our DescriptorHello object.
	// (Recall this is in DescriptorHello.h)
	DescriptorHello descHello(client, messageID, count);

	// Create a client watcher, and let libev handle the main loop.
	ev::default_loop loop;
	MCSB::ClientWatcher watcher(client, loop);
	loop.run();

	return 0;
}
