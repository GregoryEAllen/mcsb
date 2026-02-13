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

/// \file ClientOptions.h
/// \brief An options/parameters class for constructing an MCSB Client.

#ifndef MCSB_ClientOptions_h
#define MCSB_ClientOptions_h
#pragma once

#include "MCSB/dbprinter.h"

#include <stdint.h>
#include <stdio.h>
#include <string>

namespace MCSB {

/// An options/parameters class for constructing an MCSB Client.

/// ClientOptions is used to construct a Client. It tells the Client
/// how to connect to a Manager, how much memory to allocate for message
/// communication, whether to compute CRCs, etc.
/// ClientOptions also has rudimentary command-line parsing built in
/// so that MCSB can be built and tested without libvariant.
/// However, for beefy command-line parsing use VariantClientOptions.
/// \see VariantClientOptions.h
struct ClientOptions {
	/// Construct with all default parameters.
	ClientOptions(const char* argv0=0, const char* usage="");
	/// Construct with built-in parsing of argc and argv.
	ClientOptions(int argc, char* const argv[], const char* usage="");

	/// The default values for most of the client options.
	enum {
		kDefaultMinProducerBytes = (8*1024*1024),
		kDefaultMinConsumerBytes = (8*1024*1024),
		kDefaultMinProducerSlabs = 4,
		kDefaultMinConsumerSlabs = 4,
		kDefaultVerbosity = kNotice,
		kDefaultProducerNiceLevel = 0
	};
	// parameters used by clients
	size_t minProducerBytes;   ///< min number of bytes for producing messages
	size_t minConsumerBytes;   ///< min number of bytes for consuming messages
	uint32_t minProducerSlabs; ///< min number of slabs for producing messages
	uint32_t minConsumerSlabs; ///< min number of slabs for producing messages
	std::string ctrlSockName;  ///< name of control socket to connect to Manager
	std::string clientName;    ///< name of Client, to report to Manager
	char verbosity;            ///< Client verbosity level
	char producerNiceLevel;    ///< producer niceness (playback/non-realtime mode)

	/// Values for client-side message CRC computation and verification.
	typedef enum {
		eNoCrcs = 0, ///< Do not set or verify CRCs on any message.
		eSetCrcs = 1, ///< Set CRCs (on send), but do not verify (on recv).
		eVerifyCrcs = 2, ///< Verify CRCs (on recv), but do set (on send).
		eSetAndVerifyCrcs = 3, ///< Set and verify CRCs on all messages.
		eDefaultCrcs = eNoCrcs, ///< The default CRC policy (eNoCrcs).
		kDefaultCrcPolicy = eDefaultCrcs, ///< The default CRC policy.
		// the rest are not policies, but are provided for CrcPolicyStr(CrcPolicy)
		eDefaultCrcStr = 4,
		eUnknownCrcStr = 5
	} CrcPolicy;
	CrcPolicy crcPolicy; ///< policy on computing/verifying message CRCs

	/// Set the CRC Policy from a string ["OFF", "SET", "VERIFY", "ON", "DEFAULT"]
	CrcPolicy SetCrcPolicy(const char* crcPolicyStr);
	/// Get the current CRC Policy as a string.
	const char* CrcPolicyStr(void) const;

	/// Get a CRC Policy string from a CrcPolicy.
	static const char* CrcPolicyStr(CrcPolicy crcPolicy);
	/// Get a string for the default CrcPolicy.
	static const char* DefaultCrcStr(void);

	/// Parse argc and argv to fill out this ClientOptions struct.
	int Parse(int argc, char* const argv[]);
	/// Print this ClientOptions struct (with a prefix) to the specified file stream.
	void Print(const char* prefix="", FILE* f=stderr) const;

	/// Utility function which substitutes %U for the username.
	static std::string SubstituteUsername(const char* fmt);
	/// Assign to a string and return the default control socket name.
	static const std::string& SetDefaultCtrlSocketName(std::string& s)
		{ return s=SubstituteUsername("/tmp/mcsb-%U.sock"); }
	/// Set this ClientOptions to use the default control socket name.
	const std::string& SetDefaultCtrlSocketName(void)
		{ return SetDefaultCtrlSocketName(ctrlSockName); };
	/// Assign to a string and return the default client name, based on the program name.
	static const std::string& SetDefaultClientName(std::string& s, const char* argv0=0);
	/// Set this ClientOptions to use the default client name.
	const std::string& SetDefaultClientName(const char* argv0=0)
		{ return SetDefaultClientName(clientName,argv0); };

  private:
	static const char* kCrcStrs[6];
	std::string usage;
	void SetDefaults(const char* argv0);
  protected:
	/// Print a usage message to the specified file stream.
	void PrintUsage(const char* argv0, FILE* f=stderr);
	/// Extra option callback type (advanced), for use with protected Parse below.
	typedef int (*XOptCB)(int opt, const char* optarg, void* user);
	/// Parse argc and argv, calling back xocbFunc if xtraOpts are encountered (advanced).
	int Parse(int argc, char* const argv[], const char* xtraOpts, XOptCB xocbFunc=0, void* user=0);
};

/// Behaves like strtoul(), but supporting power-of-two suffixes, e.g. 3M.
unsigned long strtoul_po2suffix(const char* str);
/// Set procname with the name of the process, preferrably from argv[0].
const char* set_procname(char* procname, int len, const char* argv0=0);

} // namespace MCSB

#endif
