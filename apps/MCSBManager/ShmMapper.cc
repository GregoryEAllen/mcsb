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

#include "MCSB/ShmMapper.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace MCSB {


//-----------------------------------------------------------------------------
ShmMapper::ShmMapper(const char* nameFmt, bool force_, uint32_t blkSz, uint32_t slbSz,
	uint32_t numBlks, uint32_t mxNumBufs)
//-----------------------------------------------------------------------------
:	nameFormat(nameFmt), force(force_), blockSize(blkSz), slabSize(slbSz),
	numBlocks(numBlks), maxNumBuffers(mxNumBufs), lockErrors(0)
{
	ShmHeader init(blkSz,slbSz,numBlks,0,mxNumBufs);
	bufSize = init.TotalBufferSize();
}


//-----------------------------------------------------------------------------
ShmMapper::~ShmMapper(void)
//-----------------------------------------------------------------------------
{
	for (unsigned bufNum=0; bufNum<headerVec.size(); bufNum++) {
		if (munmap((void*)headerVec[bufNum],bufSize)) {
			fprintf(stderr,"#-- munmap: %s\n", strerror(errno));
		}
		char shmName[100];
		snprintf(shmName,sizeof(shmName),nameFormat.c_str(),bufNum);
		if (shm_unlink(shmName)) {
			fprintf(stderr,"#-- shm_unlink \"%s\": %s\n", shmName, strerror(errno));
		}
	}
}


//-----------------------------------------------------------------------------
unsigned ShmMapper::NumBuffers(unsigned n)
//-----------------------------------------------------------------------------
{
	unsigned numBufs = headerVec.size();
	if (n==numBufs)
		return n; // no change
	if (n>maxNumBuffers) {
		fprintf(stderr,"#-- refusing to create %u buffers, which is greater than maxNumBuffers (%u)\n", n, maxNumBuffers);
		return n;
	}
	if (n<numBufs) {
		fprintf(stderr,"#-- refusing to reduce the number of buffers from %u to %u\n", numBufs, n);
		return n;
	}
	for (unsigned i=numBufs; i<n; i++) {
		CreateAndMap(i);
	}
	return n;
}


//-----------------------------------------------------------------------------
char* ShmMapper::GetBlockPtr(unsigned blockID) const
//-----------------------------------------------------------------------------
{
	ldiv_t result = ldiv(blockID,numBlocks);
	if ((unsigned)result.quot>=blockVec.size()) {
		throw std::runtime_error("ShmMapper::GetBlockPtr invalid slabID");
	}
	return blockVec[result.quot] + result.rem*blockSize;
}


//-----------------------------------------------------------------------------
int ShmMapper::OpenShm(unsigned bufNum, std::string& shmName) const
//-----------------------------------------------------------------------------
{
	char name[100];
	snprintf(name,sizeof(name),nameFormat.c_str(),bufNum);
	shmName = name;

	int shmFD = shm_open(name, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (shmFD<0) {
		std::string err = "shm_open \"" + shmName + "\": ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
//	fprintf(stderr, "- opened \"%s\" as fd %d\n", shmName.c_str(), shmFD);
	return shmFD;
}


//-----------------------------------------------------------------------------
void ShmMapper::CreateAndMap(unsigned bufNum)
//-----------------------------------------------------------------------------
{
//	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
	assert(bufNum>=headerVec.size()); // make sure we're not re-creating
	
	// open the filedes (trying again on force)
	int shmFD = -1;
	std::string shmName;
	try {
		shmFD = OpenShm(bufNum,shmName);
	} catch (std::runtime_error err) {
		if (!force) throw err;
		fprintf(stderr,"#-- %s\n", err.what());
		fprintf(stderr,"--- force set, trying again\n");
		if (shm_unlink(shmName.c_str())) {
			fprintf(stderr,"#-- shm_unlink %s error: %s\n", shmName.c_str(), strerror(errno));
		}
		shmFD = OpenShm(bufNum,shmName); // try again
	}

	// ftruncate to allocate zeros
	if (ftruncate(shmFD,bufSize)) {
		close(shmFD);
		shm_unlink(shmName.c_str());
		std::string err = "ftruncate \"" + shmName + "\": ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

	// mmap it
	void* shmBase = mmap(0, bufSize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE, shmFD, 0); // whither NORESERVE?
	if (shmBase == MAP_FAILED) {
		close(shmFD);
		shm_unlink(shmName.c_str());
		std::string err = "mmap \"" + shmName + "\": ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

	if (close(shmFD)) {
		fprintf(stderr,"#-- close(%d) \"%s\": %s\n", shmFD, shmName.c_str(), strerror(errno));
	}

	// update our internal state
	ShmHeader header(blockSize,slabSize,numBlocks,0,maxNumBuffers);
	ShmHeader* hdr = (ShmHeader*)shmBase;
	BlockInfo* info = (BlockInfo*)((char*)shmBase + header.infoOffset);
	char* blk = (char*)shmBase + header.blockOffset;
	headerVec.push_back(hdr);
	infoVec.push_back(info);
	blockVec.push_back(blk);

	// initialize new hdr from header
	*hdr = header; // default operator= for ShmHeader
	hdr->bufferNumber = bufNum;

	// loop over the buffers and update numBuffers
	unsigned numBufs = bufNum+1;
	for (unsigned i=0; i<numBufs; i++) {
		headerVec[i]->numBuffers = numBufs;
	}

	assert(numBufs == headerVec.size());
	assert(numBufs == infoVec.size());
	assert(numBufs == blockVec.size());

#if 0
	fprintf(stderr, "- created \"%s\" as buffer %u\n", shmName.c_str(), bufNum);
	size_t blocksSize = refhdr->TotalBlocksSize();
	float blocksMiB = blocksSize*(bufNum+1)/1048576.;
	float totalMiB = bufSize*(bufNum+1)/1048576.;
	fprintf(stderr, "- now using %g MiB (%g MiB with overhead)\n", blocksMiB, totalMiB);
#endif

	lockErrors += LockOrTouch(shmBase, bufSize);
}


//-----------------------------------------------------------------------------
int ShmMapper::LockOrTouch(const void* addr, size_t len) const
//	This function is slow because it locks or touches lots of memory
//	It could be a good candidate for a detached thread
//	returns the number of errors, which can be accumulated
//-----------------------------------------------------------------------------
{
	// lock the memory if possible
	int res = mlock(addr,len);
	if (!res) {
		return 0;
	}

	// lock failed, try touching
	fprintf(stderr, "- unable to lock memory: %s\n", strerror(errno));
#ifdef __linux__
	fprintf(stderr, "- raise 'hard memlock' in /etc/security/limits.conf to prevent swapping\n");
#endif

	// this is only necessary if the memory wasn't locked
	int pageSize = getpagesize();
	int sum = 0;
	const char* base = (const char*) addr;
	size_t offset = 0;
	for (; offset<len; offset+=pageSize) {
		sum += base[offset];
	}
	volatile int result = sum; // prevent this loop from being optimized away
	(void) result;
	return 1;
}


} // namespace MCSB

