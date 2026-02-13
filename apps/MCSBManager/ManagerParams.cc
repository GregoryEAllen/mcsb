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

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include "MCSB/ManagerParams.h"
#include "MCSB/ClientOptions.h"
#include "MCSB/dbprinter.h"
#include "MCSB/MCSBVersion.h"

#include <unistd.h>
#include <libgen.h>
#include <limits>
#include <cstdlib>
#include <cstring>

extern const char* kHgRevision_MCSB;

namespace MCSB {

const char* kDefaultShmNameFormat = "/mcsb-%U.buf%02u";

//-----------------------------------------------------------------------------
ManagerParams::ManagerParams(void)
//-----------------------------------------------------------------------------
{
	SetDefaults();
}

//-----------------------------------------------------------------------------
ManagerParams::ManagerParams(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	SetDefaults();
	Parse(argc,argv);
}

//-----------------------------------------------------------------------------
void ManagerParams::SetDefaults(void)
//-----------------------------------------------------------------------------
{
	std::string name;
	ctrlSockName = ClientOptions::SetDefaultCtrlSocketName(name);
	verbosity = kDefaultVerbosity;
	force = 0;
	allowUnlockedMemory = 0;
	playbackMode = 0;
	maxNumClients = 100;
	backlog = 100;
	
	ShmNameFmt(kDefaultShmNameFormat);
	blockSize = kDefaultBlockSize;
	slabSize = kDefaultSlabSize;
	bufferSize = kDefaultBufferSize;
	numBlocks = bufferSize/blockSize;
	numBuffers = kDefaultNumBuffers;
	maxNumBuffers = kDefaultMaxNumBuffers;
	nonrsrvblePct = kDefaultNonrsrvblePct;
}

//-----------------------------------------------------------------------------
int ManagerParams::Parse(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	int c;
	int testAndExit = 0;
	optind = 1;
	bool ctrlSockNameSet = 0;
	bool shmNameFmtSet = 0;
	while ((c = getopt(argc,argv,"c:m:fFps:S:b:n:N:r:vh?t")) != -1) {
		switch (c) {
			case 'c':
				ctrlSockName = ClientOptions::SubstituteUsername(optarg);
				ctrlSockNameSet = 1;
				break;
			case 'm':
				shmNameFmt = ShmNameFmt(optarg);
				shmNameFmtSet = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'F':
				allowUnlockedMemory = 1;
				break;
			case 'p':
				playbackMode = 1;
				break;
			case 's':
				blockSize = strtoul_po2suffix(optarg);
				break;
			case 'S':
				slabSize = strtoul_po2suffix(optarg);
				break;
			case 'b':
				bufferSize = strtoul_po2suffix(optarg);
				break;
			case 'n':
				numBuffers = strtoul(optarg,0,0);
				break;
			case 'N':
				maxNumBuffers = strtoul(optarg,0,0);
				break;
			case 'r':
				nonrsrvblePct = strtof(optarg,0);
				break;
			case 'v':
				verbosity++;
				break;
			case 't':
				testAndExit++;
				break;
			case '?':
			case 'h':
				usage(argv[0]);
				exit(0);
			default:
				usage(argv[0]);
				exit(-1);
		}
	}
	
	if (ctrlSockNameSet && !shmNameFmtSet) {
		// make shmNameFmt based on ctrlSockName
		// get the basename of ctrlSockName
		int shmNameLen = ctrlSockName.size();
		char ctrlSockNameCopy[shmNameLen+1];
		strcpy(ctrlSockNameCopy,ctrlSockName.c_str());
		char* shmNameBase = basename(ctrlSockNameCopy);
		// strip off the extension if it is .sock
		char* ext = strrchr(shmNameBase, '.');
		if (ext && !strcmp(ext,".sock"))
			*ext = 0; // null terminate if found
		// build the result
		shmNameFmt = "/";
		shmNameFmt += shmNameBase;
		shmNameFmt += ".buf%02u";
	}

	ParamCheck();
	if (testAndExit) exit(0);
	return 0;
}

//-----------------------------------------------------------------------------
void ManagerParams::usage(const char* argv0)
//-----------------------------------------------------------------------------
{
	int procNameLen = strlen(argv0);
	char argv0copy[procNameLen+1];
	strcpy(argv0copy,argv0);
	std::string procName = basename(argv0copy);
	std::string sockName;
	ClientOptions::SetDefaultCtrlSocketName(sockName);
	std::string shmName = ShmNameFmt(kDefaultShmNameFormat);

	fprintf(stderr, "usage: %s [opts]\n", procName.c_str());
	fprintf(stderr, "  -c sockName    control socket filename [%s]\n", ctrlSockName.c_str());
	fprintf(stderr, "                 (also sets memNameFmt unless -m spec'd)\n");
	fprintf(stderr, "  -m memNameFmt  shared memory name format [%s]\n", shmName.c_str());
	fprintf(stderr, "  -f             forcibly override if socket or shared memory exists\n");
	fprintf(stderr, "  -F             proceed even if memory locking fails\n");
	fprintf(stderr, "  -p             playback/non-realtime mode (don't drop blocks)\n");
	fprintf(stderr, "  -s blockSize   smallest message size in bytes [%u]\n", kDefaultBlockSize);
	fprintf(stderr, "  -S slabSize    largest contiguous message size in bytes [%u]\n", kDefaultSlabSize);
	fprintf(stderr, "  -b bufferSize  shared memory mapping size in bytes [%u]\n", kDefaultBufferSize);
	fprintf(stderr, "  -n numBuffers  initial number of buffers [%u]\n", kDefaultNumBuffers);
	fprintf(stderr, "  -N maxNumBufs  numBuffers growable on demand to this max [%u]\n", kDefaultMaxNumBuffers);
	fprintf(stderr, "  -r nonrsrvble  percent memory non-reservable by clients [%u%%]\n", kDefaultNonrsrvblePct);
	fprintf(stderr, "  -v             increase verbosity [default %u]\n", kDefaultVerbosity);
	fprintf(stderr, "  -h             this help\n");
	fprintf(stderr, "MCSB Version %s", MCSB_VERSION);
	if (kHgRevision_MCSB && strlen(kHgRevision_MCSB)) {
		fprintf(stderr, ", Mercurial id %s", kHgRevision_MCSB);
	}
	fprintf(stderr, "\n");
}

//-----------------------------------------------------------------------------
void ManagerParams::ParamCheck(void)
//-----------------------------------------------------------------------------
{
	dbprinter p(verbosity);
	
	int lvl = kNotice;
	if (blockSize>INT32_MAX || !blockSize) {
		blockSize = kDefaultBlockSize;
		p.dbprintf(lvl, "- invalid blockSize set to default of %u\n", blockSize);
	}
	unsigned pageSize = getpagesize();
	int pagesPerBlock = (blockSize-1+pageSize)/pageSize;
	if (blockSize!=pagesPerBlock*pageSize) {
		if (pagesPerBlock<1) pagesPerBlock = 1;
		blockSize = pagesPerBlock*pageSize;
		p.dbprintf(lvl, "- rounding blockSize up to %u, or %u*pageSize\n", blockSize, pagesPerBlock);
	}

	if (slabSize>INT32_MAX || !slabSize) {
		slabSize = kDefaultSlabSize;
		p.dbprintf(lvl, "- invalid slabSize set to default of %u\n", slabSize);
	}
	int blocksPerSlab = (slabSize-1+blockSize)/blockSize;
	if (slabSize!=blocksPerSlab*blockSize) {
		if (blocksPerSlab<1) blocksPerSlab = 1;
		slabSize = blocksPerSlab*blockSize;
		p.dbprintf(lvl, "- rounding slabSize up to %u, or %u*blockSize\n", slabSize, blocksPerSlab);
	}

	if (bufferSize>INT64_MAX || !bufferSize) {
		bufferSize = kDefaultBufferSize;
		p.dbprintf(lvl, "- invalid bufferSize set to default of %u\n", bufferSize);
	}
	int slabsPerBuffer = (bufferSize-1+slabSize)/slabSize;
	if (bufferSize!=slabsPerBuffer*(unsigned long)slabSize) {
		if (slabsPerBuffer<1) slabsPerBuffer = 1;
		bufferSize = slabsPerBuffer*(unsigned long)slabSize;
		p.dbprintf(lvl, "- rounding bufferSize up to %llu, or %u*slabSize\n",
			(unsigned long long)bufferSize, slabsPerBuffer);
	}
	numBlocks = blocksPerSlab*slabsPerBuffer;
	
	unsigned kMaxNonrsrvblePct = 80;
	if (nonrsrvblePct<0) {
		nonrsrvblePct = kDefaultNonrsrvblePct;
		p.dbprintf(lvl, "- invalid nonrsrvblePct set to default of %g%%\n", nonrsrvblePct);
	} else if (nonrsrvblePct>kMaxNonrsrvblePct) {
		nonrsrvblePct = kMaxNonrsrvblePct;
		p.dbprintf(lvl, "- invalid nonrsrvblePct set to maximum of %g%%\n", nonrsrvblePct);
	}

	lvl = kInfo;
	p.dbprintf(lvl, "ManagerParams:\n");
	p.dbprintf(lvl, "  ctrlSockName: %s\n", ctrlSockName.c_str());
	p.dbprintf(lvl, "  shmNameFmt: %s\n", shmNameFmt.c_str());
	p.dbprintf(lvl, "  force: %d\n", int(force));
	p.dbprintf(lvl, "  allowUnlockedMemory: %d\n", int(allowUnlockedMemory));
	p.dbprintf(lvl, "  playbackMode: %d\n", int(playbackMode));
	p.dbprintf(lvl, "  blockSize: %u\n", blockSize);
	p.dbprintf(lvl, "  slabSize: %u\n", slabSize);
	p.dbprintf(lvl, "  bufferSize: %llu\n", (unsigned long long)bufferSize);
	p.dbprintf(lvl, "  numBlocks: %u\n", numBlocks);
	p.dbprintf(lvl, "  blocksPerSlab: %u\n", blocksPerSlab);
	p.dbprintf(lvl, "  slabsPerBuffer: %u\n", slabsPerBuffer);
	p.dbprintf(lvl, "  numBuffers: %u\n", numBuffers);
	p.dbprintf(lvl, "  maxNumBuffers: %u\n", maxNumBuffers);
	p.dbprintf(lvl, "  nonrsrvblePct: %g\n", nonrsrvblePct);
	p.dbprintf(lvl, "  verbosity: %d\n", verbosity);
	p.dbprintf(lvl, "  maxNumClients: %u\n", maxNumClients);
	p.dbprintf(lvl, "  backlog: %u\n", backlog);
}

//-----------------------------------------------------------------------------
const std::string& ManagerParams::ShmNameFmt(const char* fmt)
// sub %U for username
//-----------------------------------------------------------------------------
{
	return shmNameFmt = ClientOptions::SubstituteUsername(fmt);
}

} // namespace MCSB
