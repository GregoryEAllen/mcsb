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

#ifndef MCSB_MessageSegment_h
#define MCSB_MessageSegment_h
#pragma once

#include "MCSB/IntrusiveList.h"
#include "MCSB/ShmDefs.h"

#include <stdint.h>
#include <sys/uio.h>

namespace MCSB {

//-----------------------------------------------------------------------------
class MessageSegment {
  protected:
	void* buf;
	uint32_t size;
	uint32_t blockID;
	MessageSegment* next;
  public:
	MessageSegment(void): buf(0), size(0), blockID(0), next(0) {}
	bool Contiguous(void) const { return !next; }
	unsigned NumSegments(void) const;
	size_t TotalSize(void) const;
	int GetIovec(struct iovec iov[], int iovcnt) const;
	uint32_t Size(void) const { return size; }
	uint32_t BlockID(void) const { return blockID; }
	bool IovecMatch(const struct iovec iov[], int iovcnt) const;
};

//-----------------------------------------------------------------------------
class SendMsgSegment :
	public MCSB::MessageSegment,
	public IntrusiveList<SendMsgSegment>::Hook
{
  public:
	void* Buf(void) const { return buf; }
	const SendMsgSegment* Next(void) const
		{ return static_cast<const SendMsgSegment*>(next); }
	SendMsgSegment* Next(void)
		{ return static_cast<SendMsgSegment*>(next); }
  protected:
	friend class ClientSendManager;
};

//-----------------------------------------------------------------------------
class RecvMsgSegment :
	public MCSB::MessageSegment,
	public IntrusiveList<RecvMsgSegment>::Hook
{
  public:
	RecvMsgSegment(void): blockInfo(0), refcnt(1) {}
	const void* Buf(void) const { return buf; }
	const RecvMsgSegment* Next(void) const { return static_cast<RecvMsgSegment*>(next); }
	uint32_t MessageID(void) const { return blockInfo->messageID; }
	bool ValidCRC(bool ignoreZeros, bool allSegs=1) const;
	size_t CopyToBuffer(void* dst, size_t maxlen) const;
  protected:
	const BlockInfo* blockInfo;
	unsigned refcnt;
	friend class ClientRecvManager;
	friend class RecvMessageDescriptor;
	void inc(void) { ++refcnt; }
	unsigned dec(void) { return --refcnt; }
};

} // namespace MCSB

#endif
