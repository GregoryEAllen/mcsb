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

#include "MCSB/MessageSegment.h"
#include "MCSB/crc32c.h"

#include <stdexcept>
#include <cstring>

namespace MCSB {

//-----------------------------------------------------------------------------
unsigned MessageSegment::NumSegments(void) const
//-----------------------------------------------------------------------------
{
	unsigned numSegments = 0;
	const MessageSegment* seg = this;
	while (seg) {
		seg = seg->next;
		numSegments++;
	}
	return numSegments;
}

//-----------------------------------------------------------------------------
size_t MessageSegment::TotalSize(void) const
//-----------------------------------------------------------------------------
{
	size_t totalSize = 0;
	const MessageSegment* seg = this;
	while (seg) {
		totalSize += seg->size;
		seg = seg->next;
	}
	return totalSize;
}

//-----------------------------------------------------------------------------
int MessageSegment::GetIovec(struct iovec iov[], int iovcnt) const
//-----------------------------------------------------------------------------
{
	int count = 0;
	const MessageSegment* seg = this;
	while (seg) {
		if (count>=iovcnt) {
			// return negative of actual size needed
			return -NumSegments();
		}
		iov[count].iov_base = seg->buf;
		iov[count].iov_len = seg->size;
		count++;
		seg = seg->next;
	}
	if (count<iovcnt)
		iov[count].iov_base = 0;
	return count;
}

//-----------------------------------------------------------------------------
bool MessageSegment::IovecMatch(const struct iovec iov[], int iovcnt) const
// returns true if iov matches the segment up to the length of iov
//-----------------------------------------------------------------------------
{
	const MessageSegment* seg = this;
	for (int i=0; i<iovcnt; i++) {
		if (!seg) return 0;
		if (iov[i].iov_base != seg->buf) return 0;
		if (iov[i].iov_len > seg->size) return 0;
		seg=seg->next;
	}
	return 1;
}

//-----------------------------------------------------------------------------
bool RecvMsgSegment::ValidCRC(bool ignoreZeros, bool allSegs) const
//-----------------------------------------------------------------------------
{
	const RecvMsgSegment* seg = this;
	while (seg) {
		if (ignoreZeros && !seg->blockInfo->crc32c) {
			// ignore zeros because the crc32c was likely not set
		} else {
			uint32_t crc = crc32c(seg->buf,seg->size);
			if (crc != seg->blockInfo->crc32c) return 0;
		}
		if (!allSegs) break;
		seg = static_cast<RecvMsgSegment*>(seg->next);
	}
	return 1;
}

//-----------------------------------------------------------------------------
size_t RecvMsgSegment::CopyToBuffer(void* dst, size_t maxlen) const
//-----------------------------------------------------------------------------
{
	size_t totalLen = 0;
	const RecvMsgSegment* seg = this;
	while (seg) {
		uint32_t bytesToCopy = seg->size;
		if (totalLen+bytesToCopy>maxlen)
			bytesToCopy = maxlen-totalLen;
		totalLen += bytesToCopy;
		memcpy(dst, seg->buf, bytesToCopy);
		dst = (char*)dst+bytesToCopy;
		seg = static_cast<RecvMsgSegment*>(seg->next);
	}
	return totalLen;
}

} // namespace MCSB
