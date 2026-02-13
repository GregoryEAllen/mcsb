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

#include "MCSB/MessageDescriptors.h"
#include "MCSB/ClientImpl.h"
#include "MCSB/dbprinter.h"
#include <stdexcept>
#include <memory>
#include <cstring>

namespace MCSB {

//-----------------------------------------------------------------------------
bool MessageDescriptor::Contiguous(void) const
{ return seg->Contiguous(); }
unsigned MessageDescriptor::NumSegments(void) const
{ return seg->NumSegments(); }
uint32_t MessageDescriptor::TotalSize(void) const
{ return seg->TotalSize(); }
int MessageDescriptor::GetIovec(struct iovec iov[], int iovcnt) const
{ return seg->GetIovec(iov,iovcnt); }
uint32_t MessageDescriptor::Size(void) const
{ return seg->Size(); }
uint32_t MessageDescriptor::BlockID(void) const
{ return seg->BlockID(); }
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void* SendMessageDescriptor::Buf(void) const
{ return static_cast<SendMsgSegment*>(seg)->Buf(); }
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
SendMessageDescriptor::~SendMessageDescriptor(void)
//-----------------------------------------------------------------------------
{
	Reset(0,0);
}

//-----------------------------------------------------------------------------
void SendMessageDescriptor::Reset(SendMsgSegment* s, ClientImpl* c)
//-----------------------------------------------------------------------------
{
	if (seg && cimpl) {
		int verbosity = cimpl->Verbosity();
		try {
			if (cimpl->Connected()) {
				// writes to sock, may throw
				cimpl->ReleaseSendMsgDesc(static_cast<SendMsgSegment*>(seg));
			}
		} catch (std::runtime_error err) {
			dbprinter p(verbosity);
			p.dbprintf(kError, "#-- %s\n", err.what());
		}
	}
	seg = s;
	cimpl = c;
}

//-----------------------------------------------------------------------------
SendMsgSegment* SendMessageDescriptor::Release(void)
//-----------------------------------------------------------------------------
{
	SendMsgSegment* s = static_cast<SendMsgSegment*>(seg);
	seg = 0;
	return s;
}

//-----------------------------------------------------------------------------
int SendMessageDescriptor::SendMessage(uint32_t msgID, uint32_t len)
//-----------------------------------------------------------------------------
{
	int verbosity = 0;
	try {
		if (cimpl) {
			verbosity = cimpl->Verbosity();
			return cimpl->SendMessage(msgID,Release(),len);
		}
	} catch (std::runtime_error err) {
		dbprinter p(verbosity);
		p.dbprintf(kError, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
void SendMessageDescriptor::Fill(uint8_t val)
//-----------------------------------------------------------------------------
{
	SendMsgSegment* s = static_cast<SendMsgSegment*>(seg);
	while(s) {
		::memset(s->Buf(),val,s->Size());
		s = s->Next();
	}
}

//-----------------------------------------------------------------------------
uint32_t RecvMessageDescriptor::MessageID(void) const
{ return static_cast<RecvMsgSegment*>(seg)->MessageID(); }
const void* RecvMessageDescriptor::Buf(void) const
{ return static_cast<RecvMsgSegment*>(seg)->Buf(); }
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
RecvMessageDescriptor& RecvMessageDescriptor::operator=(const RecvMessageDescriptor& desc)
//-----------------------------------------------------------------------------
{
	RecvMessageDescriptor rmd(desc);
	dec();
	seg = desc.seg;
	cimpl = desc.cimpl;
	inc();
	return *this;
}

//-----------------------------------------------------------------------------
void RecvMessageDescriptor::inc(void)
//-----------------------------------------------------------------------------
{
	if (seg) static_cast<RecvMsgSegment*>(seg)->inc();
}

//-----------------------------------------------------------------------------
void RecvMessageDescriptor::dec(void)
//-----------------------------------------------------------------------------
{
	if (!seg) return;
	RecvMsgSegment* rms = static_cast<RecvMsgSegment*>(seg);
	if (!rms->dec()) {
		int verbosity = cimpl->Verbosity();
		try {
			if (cimpl->Connected()) {
				// writes to sock, may throw
				cimpl->ReleaseRecvMsgDesc(rms);
			}
		} catch (std::runtime_error err) {
			dbprinter p(verbosity);
			p.dbprintf(kError, "#-- %s\n", err.what());
		}
	}
}

//-----------------------------------------------------------------------------
std::pair<char*,ptrdiff_t> GetTempBufferFromRMD(const RecvMessageDescriptor& desc)
//-----------------------------------------------------------------------------
{
	uint32_t len = desc.TotalSize();
	std::pair<char*,ptrdiff_t> tmp = std::get_temporary_buffer<char>(len);
	if ((long)len != tmp.second) {
		// get_temporary_buffer shorted us
		throw std::runtime_error("std::get_temporary_buffer returned insufficient memory");
	}
	MessageSegment* seg = desc.Seg();
	static_cast<RecvMsgSegment*>(seg)->CopyToBuffer(tmp.first,tmp.second);
	return tmp;
}

} // namespace MCSB
