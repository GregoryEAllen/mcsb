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

#ifndef MCSB_ClientSendManager_h
#define MCSB_ClientSendManager_h
#pragma once

#include "MCSB/IntrusiveList.h"
#include "MCSB/MessageSegment.h"

#include <stdint.h>
#include <sys/uio.h>
#include <vector>

namespace MCSB {

class SendMsgSegment;

// this class holds a pool of producer slabs and manages the policy
// for allocating space in the pool for sending messages

class ClientSendManager {
  public:
	ClientSendManager(void);
	~ClientSendManager(void);

	void SetSizeParams(uint32_t blockSz, uint32_t slabSz);
	void SetNumTotalSlabs(uint32_t numTotalSlabs);
	unsigned MaxWorkingSlabs(unsigned mxWorkingSlabs);
	unsigned MaxWorkingSlabs(void) const { return maxWorkingSlabs; }
	void FlushWorkingSlabs(void); // discard all of them

	void AddFreeSlabs(const uint32_t slabIDs[], void* slabPtrs[], unsigned count);
	unsigned GetRetiredSlabs(uint32_t slabIDs[], unsigned maxCount); // returns count

	unsigned SlabsHeld(void) const { return slabsHeld; }
	unsigned NumFreeSlabs(void) const { return freeSlabs.size(); }
	unsigned NumWorkingSlabs(void) const { return workingSlabs.size(); }
	unsigned NumFullSlabs(void) const { return fullSlabs.size(); }
	unsigned NumRetiredSlabs(void) const { return retiredSlabs.size(); }

	typedef SendMsgSegment* MessageDescriptor; // linked-list of SendMsgSegment
	MessageDescriptor GetMessageDescriptor(uint32_t len, bool contiguous=true); // null on fail
	void ReleaseMessageDescriptor(MessageDescriptor desc);

	class DescriptorReleaser;

  protected:
	uint32_t blockSize;
	uint32_t slabSize;
	unsigned blocksPerSlab;
	unsigned maxWorkingSlabs;
	unsigned slabsHeld;

	SendMsgSegment* GetSegment(unsigned nBlocks, bool noFreeSlabs=0);

	class SlabInfo;
	typedef IntrusiveList<SlabInfo> SlabList;

	// slabs move from freeSlabs -> workingSlabs -> fullSlabs -> retiredSlabs
	SlabList freeSlabs;    // slabs completely unused
	SlabList workingSlabs; // where allocation is currently occurring
	SlabList fullSlabs;    // completely allocated, but non-zero refcount
	SlabList retiredSlabs; // slabs no longer in use

	typedef std::vector<SlabInfo*> SlabInfoVec;
	SlabInfoVec slabInfoVec;
	
	typedef IntrusiveList<SendMsgSegment> MsgSegList;
	MsgSegList freeSegs;
	MsgSegList busySegs;

	std::vector<SendMsgSegment*> segs;
	void AddMsgSegBlock(void);
};

//-----------------------------------------------------------------------------
class ClientSendManager::SlabInfo : public IntrusiveList<SlabInfo>::Hook {
	uint32_t slabID;
	unsigned blocksUsed; // where allocation will occur
	unsigned refcount;   // the number of runs allocated in slab
	bool working;
	void* buf;
  public:
	SlabInfo(uint32_t sid): slabID(sid) { Reset(0); }
	void Reset(void* p) { blocksUsed=0; refcount=0; working=0; buf=p; }
	uint32_t SlabID(void) const { return slabID; }
	bool Working(bool b) { return working = b; }
	bool Working(void) const { return working; }
	unsigned BlocksUsed(void) const { return blocksUsed; }
	unsigned Alloc(unsigned blocks); // returns offset
	unsigned DecRef(void); // returns new ref
	unsigned Refcount(void) const { return refcount; }
	void* Buf(void) const { return buf; }
};

//-----------------------------------------------------------------------------
class ClientSendManager::DescriptorReleaser  {
	MessageDescriptor desc;
	ClientSendManager& sendMgr;
  public:
	DescriptorReleaser(MessageDescriptor d, ClientSendManager& mgr)
		: desc(d), sendMgr(mgr) {}
	~DescriptorReleaser(void)
		{ sendMgr.ReleaseMessageDescriptor(desc); }
};

} // namespace MCSB

#endif
