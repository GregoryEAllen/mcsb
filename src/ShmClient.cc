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

#include "MCSB/ShmClient.h"

#include <unistd.h>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>

namespace MCSB {


//-----------------------------------------------------------------------------
ShmClient::ShmClient(const char* nameFmt)
//-----------------------------------------------------------------------------
:	nameFormat(nameFmt), blockSize(0), slabSize(0), numBlocks(0),
	bufSize(0), blksSize(0), blocksPerSlab(0)
{
	if (nameFormat.size())
		Initialize();
}


//-----------------------------------------------------------------------------
void ShmClient::NameFormat(const char* nameFmt)
//-----------------------------------------------------------------------------
{
	std::string newNameFormat(nameFmt);

	if (nameFormat.size()) {
		if (nameFormat==newNameFormat) {
			NumBuffers(); // check for more buffers
			return;
		} else {
			throw std::runtime_error("ShmClient::NameFormat changed");
		}
	}
	nameFormat = nameFmt;
	Initialize();
}


//-----------------------------------------------------------------------------
void ShmClient::Initialize(void)
//-----------------------------------------------------------------------------
{
	ShmHeader hdr;
	GetFirstHeader(hdr);
	blockSize = hdr.blockSize;
	slabSize = hdr.slabSize;
	numBlocks = hdr.numBlocks;
	bufSize = hdr.TotalBufferSize();
	blksSize = hdr.TotalBlocksSize();
	Map(0, &hdr);
	MapBuffers();
	blocksPerSlab = SlabSize()/BlockSize();
}


//-----------------------------------------------------------------------------
ShmClient::~ShmClient(void)
//-----------------------------------------------------------------------------
{
	for (unsigned bufNum=0; bufNum<headerVec.size(); bufNum++) {
		if (munmap((void*)headerVec[bufNum],bufSize)) {
			fprintf(stderr,"#-- munmap: %s\n", strerror(errno));
		}
		if (munmap((void*)blockVecWrt[bufNum],blksSize)) {
			fprintf(stderr,"#-- munmap: %s\n", strerror(errno));
		}
	}
}


//-----------------------------------------------------------------------------
unsigned ShmClient::MapBuffers(void)
//-----------------------------------------------------------------------------
{
	unsigned bufsMapped = headerVec.size();
	if (!bufsMapped) return bufsMapped;
	volatile unsigned bufsAvailable = headerVec[0]->numBuffers;
	if (bufsMapped==bufsAvailable)
		return bufsMapped; // no change
	if (bufsMapped>bufsAvailable) {
		throw std::runtime_error("ShmClient numBuffers decreased");
	}

	unsigned i=bufsMapped;
	while (i<bufsAvailable) {
		Map(i++,headerVec[0]);
	}
	assert(bufsAvailable == headerVec[0]->numBuffers);
	return bufsAvailable;
}


//-----------------------------------------------------------------------------
const BlockInfo* ShmClient::GetBlockInfo(unsigned blockID) const
//-----------------------------------------------------------------------------
{
	ldiv_t result = ldiv(blockID,numBlocks);
	return GetBlockInfo(result.quot,result.rem);
}


//-----------------------------------------------------------------------------
const char* ShmClient::GetBlockPtr(unsigned blockID) const
//-----------------------------------------------------------------------------
{
	ldiv_t result = ldiv(blockID,numBlocks);
	return GetBlockPtr(result.quot,result.rem);
}


//-----------------------------------------------------------------------------
char* ShmClient::GetWriteableBlockPtr(unsigned blockID) const
//-----------------------------------------------------------------------------
{
	ldiv_t result = ldiv(blockID,numBlocks);
	return GetWriteableBlockPtr(result.quot,result.rem);
}


//-----------------------------------------------------------------------------
int ShmClient::OpenShm(unsigned bufNum, std::string& shmName) const
//-----------------------------------------------------------------------------
{
	char name[100];
	snprintf(name,sizeof(name),nameFormat.c_str(),bufNum);
	shmName = name;

	int shmFD = shm_open(name, O_RDWR, S_IRUSR|S_IRGRP|S_IROTH);
	if (shmFD<0) {
		std::string err = "shm_open \"" + shmName + "\": ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
//	fprintf(stderr, "- opened \"%s\" as fd %d\n", shmName.c_str(), shmFD);
	return shmFD;
}


//-----------------------------------------------------------------------------
void ShmClient::GetFirstHeader(ShmHeader& hdr)
//-----------------------------------------------------------------------------
{
	std::string shmName;
	int shmFD = OpenShm(0,shmName);
	
	// map the header
	void* shmBase = mmap(0, ShmHeader::PaddedSize(), PROT_READ, MAP_SHARED|MAP_NORESERVE, shmFD, 0);
	if (shmBase == MAP_FAILED) {
		close(shmFD);
		std::string err = "mmap \"" + shmName + "\": ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	const ShmHeader* shmHdr = (const ShmHeader*)shmBase;
	
	// verify that header is valid
	for (int i=0; i<100; i++) {
		if (shmHdr->ValidHeader()) break;
		fprintf(stderr,"#-- shared memory header invalid, trying again\n");
		usleep(10);
	}
	if (!shmHdr->ValidHeader()) {
		throw std::runtime_error("shared memory header invalid");
	}
	hdr = *((ShmHeader*)shmBase);

	// unmap the header
	munmap(shmBase,ShmHeader::PaddedSize());
	
	if (close(shmFD)) {
		fprintf(stderr,"#-- close(%d) \"%s\": %s\n", shmFD, shmName.c_str(), strerror(errno));
	}
}


//-----------------------------------------------------------------------------
void ShmClient::Map(unsigned bufNum, const ShmHeader* refhdr)
//-----------------------------------------------------------------------------
{
//	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);

	int shmFD = -1;
	std::string shmName;
	shmFD = OpenShm(bufNum,shmName);

	// mmap RO the whole thing
	void* shmBaseRO = mmap(0, bufSize, PROT_READ, MAP_SHARED|MAP_NORESERVE, shmFD, 0);
	if (shmBaseRO == MAP_FAILED) {
		std::string err = "mmap RO \"" + shmName + "\": ";
		err += strerror(errno);
		close(shmFD);
		throw std::runtime_error(err);
	}
	// mmap RW just the blocks
	void* shmBaseRW = mmap(0, blksSize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE, shmFD, refhdr->blockOffset);
	if (shmBaseRW == MAP_FAILED) {
		std::string err = "mmap RW \"" + shmName + "\": ";
		err += strerror(errno);
		munmap(shmBaseRO,bufSize);
		close(shmFD);
		throw std::runtime_error(err);
	}

	// update our internal state
	assert(bufNum == headerVec.size());
	assert(bufNum == infoVec.size());
	assert(bufNum == blockVec.size());
	const ShmHeader* hdr = (const ShmHeader*)shmBaseRO;
	const BlockInfo* info = (const BlockInfo*)((const char*)shmBaseRO + refhdr->infoOffset);
	const char* blk = (const char*)shmBaseRO + refhdr->blockOffset;
	headerVec.push_back(hdr);
	infoVec.push_back(info);
	blockVec.push_back(blk);

	// update more internal state
	assert(bufNum == blockVecWrt.size());
	blockVecWrt.push_back((char*)shmBaseRW);

	if (close(shmFD)) {
		fprintf(stderr,"#-- close(%d) \"%s\": %s\n", shmFD, shmName.c_str(), strerror(errno));
	}

//	fprintf(stderr, "- attached to \"%s\" as buffer %u\n", shmName.c_str(), bufNum);
}


} // namespace MCSB
