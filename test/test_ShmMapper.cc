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

// always assert for tests, even in Release
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "MCSB/ShmMapper.h"
#include "MCSB/ShmClient.h"

#include <cstdio>
#include <cassert>
#include <stdexcept>

#include "rand_buf.h"

//-----------------------------------------------------------------------------
int main()
//-----------------------------------------------------------------------------
{
	uint32_t blkSz = 4096;
	uint32_t slbSz = blkSz*16;
	uint32_t blksPerBuf = 64;
	uint32_t numBufs = 4;
	uint32_t maxNumBufs = 16;
	const char* nameFmt = "/MCSB-buf%02u.mem";
	unsigned errs = 0;

	try {
		bool force = 1;
		MCSB::ShmMapper mapper(nameFmt, force, blkSz, slbSz, blksPerBuf, maxNumBufs);
		mapper.NumBuffers(numBufs);
		MCSB::ShmClient client(nameFmt);
		
		for (unsigned i=mapper.NumBuffers(); i<=maxNumBufs; i++) {
			mapper.NumBuffers(i);
			client.MapBuffers();
		}

		mapper.NumBuffers(maxNumBufs-1);
		assert(mapper.NumBuffers()==maxNumBufs);
		mapper.NumBuffers(maxNumBufs+1);
		assert(mapper.NumBuffers()==maxNumBufs);

		// verify that ShmHeader mapped
		for (unsigned i=0; i<maxNumBufs; i++) {
			MCSB::ShmHeader* mapperHdr = mapper.GetShmHeader(i);
			const MCSB::ShmHeader* clientHdr = client.GetShmHeader(i);
			assert(mapperHdr->bufferNumber == i);
			assert(clientHdr->bufferNumber == i);
			assert(clientHdr->blockSize == blkSz);
			assert(clientHdr->numBlocks == blksPerBuf);
			assert(clientHdr->maxNumBuffers == maxNumBufs);
		}

		// verify that BlockInfo mapped
		uint32_t totNumBlocks = blksPerBuf*maxNumBufs;
		uint32_t seed = 1;
		for (unsigned i=0; i<totNumBlocks; i++) {
			MCSB::BlockInfo* mapperBi = mapper.GetBlockInfo(i);
			const MCSB::BlockInfo* clientBi = client.GetBlockInfo(i);
			errs += MCSB::set_and_verify_rand_buf(mapperBi, clientBi, sizeof(MCSB::BlockInfo), seed);
			if (errs) {
				fprintf(stderr,"### %d errors while verifying BlockInfo at block %d\n", errs, i);
			}
		}

		// verify that blocks mapped
		for (unsigned i=0; i<totNumBlocks; i++) {
			char* mapperBp = mapper.GetBlockPtr(i);
			const char* clientBp = client.GetBlockPtr(i);
			errs += MCSB::set_and_verify_rand_buf(mapperBp, clientBp, blkSz, seed);
			if (errs) {
				fprintf(stderr,"### %d errors while verifying at block %d\n", errs, i);
			}
		}

		// verify they also work from the client side
		for (unsigned i=0; i<totNumBlocks; i++) {
			char* clientBp = client.GetWriteableBlockPtr(i);
			const char* mapperBp = mapper.GetBlockPtr(i);
			errs += MCSB::set_and_verify_rand_buf(clientBp, mapperBp, blkSz, seed);
			if (errs) {
				fprintf(stderr,"### %d errors while verifying at block %d\n", errs, i);
			}
		}
		if (errs) return errs;

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}

	// make sure the mapper cleaned up
	try {
		MCSB::ShmClient client(nameFmt);
		assert(0);
	} catch (std::runtime_error err) {
	}

	// make sure we can do the same with default args
	bool force = 1;
	MCSB::ShmMapper mapper(nameFmt, force, blkSz, slbSz, blksPerBuf, maxNumBufs);
	mapper.NumBuffers(numBufs);
	try {
		MCSB::ShmClient client;
		assert(client.NumBuffers()==0);
		client.NameFormat(nameFmt);
		mapper.NumBuffers(numBufs+1);
		client.NameFormat(nameFmt);
		client.MapBuffers();
		assert(client.NumBuffers()==numBufs+1);
		uint32_t totNumBlocks = blksPerBuf*numBufs;
		uint32_t seed = 1;
		// verify that blocks mapped
		for (unsigned i=0; i<totNumBlocks; i++) {
			char* mapperBp = mapper.GetBlockPtr(i);
			const char* clientBp = client.GetBlockPtr(i);
			errs += MCSB::set_and_verify_rand_buf(mapperBp, clientBp, blkSz, seed);
			if (errs) {
				fprintf(stderr,"### %d errors while verifying at block %d\n", errs, i);
			}
		}

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}

	try {
		MCSB::ShmClient client;
		client.NameFormat(nameFmt);
		client.NameFormat("foo");
	} catch (std::runtime_error err) {
	}

	fprintf(stderr, "==== PASS ====\n");

	return 0;
}
