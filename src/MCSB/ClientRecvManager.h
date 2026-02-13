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

#ifndef MCSB_ClientRecvManager_h
#define MCSB_ClientRecvManager_h
#pragma once

#include "MCSB/MessageSegment.h"

#include <stdint.h>
#include <sys/uio.h>
#include <vector>

namespace MCSB {

// this class holds the list of messages for the client to receive

class ClientRecvManager {
  public:
	ClientRecvManager(void);
	~ClientRecvManager(void);

	void AddSegments(const uint32_t blockIDs[], const void* blockPtrs[],
		const BlockInfo* blockInfoPtrs[], unsigned count);
	unsigned GetRetiredSegments(uint32_t blockIDs[], unsigned maxCount); // returns count
	unsigned NumFreeSegments(void) const { return freeSegs.size(); }
	unsigned NumPendingSegments(void) const { return pendingSegs.size(); }
	unsigned NumBusySegments(void) const { return busySegs.size(); }
	unsigned NumRetiredSegments(void) const { return retiredSegs.size(); }

	typedef RecvMsgSegment* MessageDescriptor; // linked-list of RecvMsgSegment
	bool PendingMessage(void);
	MessageDescriptor GetMessageDescriptor(void); // next message
	void ReleaseMessageDescriptor(MessageDescriptor desc);

	uint64_t NumSegmentsRcvd(void) const { return numSegmentsRcvd; }
	unsigned NumConsSlabs(unsigned n) { return numConsSlabs=n; }

	void PrintState(const char* prefix="") const;

  protected:
	typedef IntrusiveList<RecvMsgSegment> MsgSegList;
	MsgSegList freeSegs;    // unused/empty
	MsgSegList pendingSegs; // received but unchecked
	MsgSegList busySegs;    // checked and potentially in use
	MsgSegList retiredSegs; // ready to be sent back to manager
	RecvMsgSegment* pendingMsg; // helper for PendingMessages
	uint32_t numConsSlabs;
	uint64_t numSegmentsRcvd;
	std::vector<RecvMsgSegment*> segs;
	void AddMsgSegBlock(void);
};

} // namespace MCSB

#endif
