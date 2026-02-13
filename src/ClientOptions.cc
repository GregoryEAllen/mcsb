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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <libgen.h>
#include <getopt.h>
#include <limits.h>

#include <stdexcept>

//---- support for system-specific set_procname()
#if __APPLE__
#include <mach-o/dyld.h>
#endif
#if __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace MCSB {

const char* ClientOptions::kCrcStrs[] = {
	"OFF", "SET", "VERIFY", "ON", "DEFAULT", "UNKNOWN"
};

//-----------------------------------------------------------------------------
ClientOptions::ClientOptions(const char* argv0, const char* usage_)
//-----------------------------------------------------------------------------
:	usage(usage_)
{
	SetDefaults(argv0);
}

//-----------------------------------------------------------------------------
void ClientOptions::SetDefaults(const char* argv0)
//-----------------------------------------------------------------------------
{
	minProducerBytes = kDefaultMinProducerBytes;
	minConsumerBytes = kDefaultMinConsumerBytes;
	minProducerSlabs = kDefaultMinProducerSlabs;
	minConsumerSlabs = kDefaultMinConsumerSlabs;
	SetDefaultCtrlSocketName();
	SetDefaultClientName(argv0);
	verbosity = kDefaultVerbosity;
	producerNiceLevel = kDefaultProducerNiceLevel;
	crcPolicy = kDefaultCrcPolicy;
}

//-----------------------------------------------------------------------------
ClientOptions::ClientOptions(int argc, char* const argv[], const char* usage_)
//-----------------------------------------------------------------------------
:	usage(usage_)
{
	SetDefaults(argv[0]);
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
void ClientOptions::PrintUsage(const char* argv0, FILE* f)
//-----------------------------------------------------------------------------
{
	char procname[PATH_MAX+2];
	strncpy(procname,argv0,PATH_MAX);
	const char* ufmt = "usage: %s [options]";
	if (usage.size()) ufmt = usage.c_str();
	fprintf(f, ufmt, basename(procname));
	fprintf(f, "\n");
	fprintf(f, "  -b uint   minProducerBytes [%u]\n", kDefaultMinProducerBytes);
	fprintf(f, "  -B uint   minConsumerBytes [%u]\n", kDefaultMinConsumerBytes);
	fprintf(f, "  -s uint   minProducerSlabs [%u]\n", kDefaultMinProducerSlabs);
	fprintf(f, "  -S uint   minConsumerSlabs [%u]\n", kDefaultMinConsumerSlabs);
	fprintf(f, "  -c str    ctrlSockName [\"%s\"]\n", ctrlSockName.c_str());
	fprintf(f, "  -n str    clientName [\"%s\"]\n", clientName.c_str());
	fprintf(f, "  -p str    crcPolicy string [\"%s\"]\n", DefaultCrcStr());
	fprintf(f, "  -v        increase verbosity\n");
}

//-----------------------------------------------------------------------------
int ClientOptions::Parse(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	return Parse(argc,argv,0);
}

//-----------------------------------------------------------------------------
int ClientOptions::Parse(int argc, char* const argv[],
	const char* xtraOpts, XOptCB xocbFunc, void* user)
//-----------------------------------------------------------------------------
{
	int c;
	std::string optstring = ":b:B:s:S:c:n:i:p:vh?";
	if (xtraOpts)
		optstring += xtraOpts;
	optind = 1;
	while ((c = getopt(argc, argv, optstring.c_str())) != -1) {
		switch (c) {
			case 'b':
				minProducerBytes = strtoul_po2suffix(optarg);
				break;
			case 'B':
				minConsumerBytes = strtoul_po2suffix(optarg);
				break;
			case 's':
				minProducerSlabs = strtoul(optarg,0,0);
				break;
			case 'S':
				minConsumerSlabs = strtoul(optarg,0,0);
				break;
			case 'c':
				ctrlSockName = SubstituteUsername(optarg);
				break;
			case 'n':
				clientName = optarg;
				break;
			case 'i':
				//statusReportInterval = strtof(optarg,0);
				break;
			case 'p':
				SetCrcPolicy(optarg);
				break;
			case 'v':
				verbosity++;
				break;
			case '?':
			case 'h':
				if (xocbFunc)
					return (*xocbFunc)(c,optarg,user);
				PrintUsage(argv[0]);
				return 1;
			case ':':
				PrintUsage(argv[0]);
				return -1;
			default:
				if (xocbFunc) {
					if (!(*xocbFunc)(c,optarg,user)) {
						break;
					} else {
						return -1;
					}
				}
				PrintUsage(argv[0]);
				return -1;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
void ClientOptions::Print(const char* prefix, FILE* f) const
//-----------------------------------------------------------------------------
{
	fprintf(f, "%sminProducerBytes: %lu\n", prefix, (unsigned long)minProducerBytes);
	fprintf(f, "%sminConsumerBytes: %lu\n", prefix, (unsigned long)minConsumerBytes);
	fprintf(f, "%sminProducerSlabs: %u\n", prefix, minProducerSlabs);
	fprintf(f, "%sminConsumerSlabs: %u\n", prefix, minConsumerSlabs);
	fprintf(f, "%sctrlSockName: \"%s\"\n", prefix, ctrlSockName.c_str());
	fprintf(f, "%sclientName: \"%s\"\n", prefix, clientName.c_str());
	fprintf(f, "%scrcPolicyStr: %s\n", prefix, CrcPolicyStr());
	fprintf(f, "%sverbosity: %u\n", prefix, verbosity);
	fprintf(f, "%sproducerNiceLevel: %u\n", prefix, producerNiceLevel);
}

//-----------------------------------------------------------------------------
std::string ClientOptions::SubstituteUsername(const char* fmt)
// assign s to fmt, substituting %U for username
//-----------------------------------------------------------------------------
{
	const char* ptr = strstr(fmt, "%U");
	if (!ptr) {
		return fmt;
	}

	// find the username, assign to user
	const char* user = getenv("USER");
	if (!user) {
		uid_t uid = getuid();
		errno = 0;
		struct passwd* p = getpwuid( uid );
		if (!p) {
			perror("getpwuid error");
		}
		else {
			user = p->pw_name;
		}
	}
	if (!user)
		user = getlogin();
	if (!user)
		user = "unknown";

	// build the substituted string
	std::string s(fmt,ptr-fmt); // fmt preceding %U
	s += user;
	ptr += 2; // skip %U
	s += ptr; // fmt following %U
	return s;
}

//-----------------------------------------------------------------------------
const char* ClientOptions::CrcPolicyStr(void) const
//-----------------------------------------------------------------------------
{
	if (crcPolicy<eNoCrcs || crcPolicy>eSetAndVerifyCrcs)
		return kCrcStrs[eUnknownCrcStr];
	return kCrcStrs[crcPolicy];
}

//-----------------------------------------------------------------------------
ClientOptions::CrcPolicy ClientOptions::SetCrcPolicy(const char* crcPolicyStr)
//-----------------------------------------------------------------------------
{
	if (!strncmp(crcPolicyStr,kCrcStrs[eDefaultCrcStr],10)) {
		return crcPolicy = eDefaultCrcs;
	}
	for (unsigned i=eNoCrcs; i<=eSetAndVerifyCrcs; i++) {
		if (!strncmp(crcPolicyStr,kCrcStrs[i],10)) {
			return crcPolicy = (CrcPolicy)i;
		}
	}

	// unknown string specified
	fprintf(stderr, "### Unknown CRC policy \"%s\" specified, using default \"%s\"\n", crcPolicyStr, DefaultCrcStr());
	fprintf(stderr, "### CRC policy options are ON, OFF, SET, VERIFY, or DEFAULT\n");
	return eDefaultCrcs;
}

//-----------------------------------------------------------------------------
const char* ClientOptions::CrcPolicyStr(CrcPolicy crcPolicy)
//-----------------------------------------------------------------------------
{
	if ((int)crcPolicy>eDefaultCrcStr) {
		fprintf(stderr, "### Unknown CRC policy specified, using default \"%s\"\n", DefaultCrcStr());
		return DefaultCrcStr();
	}
	return kCrcStrs[crcPolicy];
}

//-----------------------------------------------------------------------------
const char* ClientOptions::DefaultCrcStr(void)
//-----------------------------------------------------------------------------
{
	return kCrcStrs[eDefaultCrcs];
}

//-----------------------------------------------------------------------------
const std::string& ClientOptions::SetDefaultClientName(std::string& s, const char* argv0)
//-----------------------------------------------------------------------------
{
	char procname[PATH_MAX+2] = {0};
	set_procname(procname,sizeof(procname),argv0);
	s = basename(procname);
	char str[80];
	sprintf(str,"[%d]", int(getpid()));
	s += str;
	return s;
}

//-----------------------------------------------------------------------------
unsigned long strtoul_po2suffix(const char* str)
// parses %g%c where the suffix is K M G or T
// verifies the result fits in an unsigned long
// returns -1 on error
//-----------------------------------------------------------------------------
{
	char* endptr = 0;
	double d = strtod(str,&endptr);
	unsigned long multiplier = 1;

	if (*endptr) { // there is a suffix
		char suffix = *endptr++;
		if (*endptr) return -1; // more than 1 char suffix
		switch (suffix) {
			case 'k':
			case 'K':
				multiplier = 1024;
				break;
			case 'M':
				multiplier = 1024*1024;
				break;
			case 'G':
				multiplier = 1024*1024*1024;
				break;
			case 'T':
				multiplier = 1024UL*1024*1024*1024;
				break;
			default:
				d = -1;
				break;
		}
	}

	// check that the result is an unsigned long
	unsigned long result = (unsigned long)(d * multiplier);
	if (result != d * multiplier) {
		result = -1;
	}

	return result;
}

//-----------------------------------------------------------------------------
const char* set_procname(char* procname, int len, const char* argv0)
//-----------------------------------------------------------------------------
{
	#if __linux__
	if (!argv0 || !strlen(argv0)) {
		char* buf = procname;
		size_t bufsize = len;
		if (0 == readlink("/proc/self/exe", buf, bufsize)) {
			argv0 = procname;
		}
	}
	#endif /* __linux__ */

	#if __APPLE__
	if (!argv0 || !strlen(argv0)) {
		char* buf = procname;
		uint32_t bufsize = len;
		if (0 == _NSGetExecutablePath(buf, &bufsize)) {
			argv0 = procname;
		}
	}
	#endif /* __APPLE__ */

	#if __FreeBSD__
	if (!argv0 || !strlen(argv0)) {
		int mib[4];
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PATHNAME;
		mib[3] = -1; // For KERN_PROC_PATHNAME, a process ID of -1 implies the current process.
		char *buf = procname;
		size_t cb = len;
		if (0 == sysctl(mib, 4, buf, &cb, NULL, 0)) {
			argv0 = procname;
		}
	}
	#endif

	if (!argv0 || !strlen(argv0))
		argv0 = getenv("_");
	if (!argv0 || !strlen(argv0))
		argv0 = "DefaultClientName";

	// get procname from argv0
	if (procname != argv0)
		strncpy(procname,argv0,PATH_MAX);

	return procname;
}

} // namespace MCSB
