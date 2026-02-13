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

/// \file MessageDescriptors.h
/// \brief Contains the message descriptor classes used in MCSB's public API.
///
/// Message descriptors are the elemental method for a client to send or
/// receive messages. Other methods for sending and receiving messages
/// are built on top of message descriptors.
///
/// Use of the message descriptor API allows zero-copy sending and receiving
/// of messages, out of order retirement of receive messages, and multiple
/// outstanding send buffers.

#ifndef MCSB_MessageDescriptors_h
#define MCSB_MessageDescriptors_h
#pragma once

#include <utility>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/uio.h>

namespace MCSB {

// internal details opaque to public API via these classes
class ClientImpl;
class MessageSegment;
class SendMsgSegment;
class RecvMsgSegment;

/// MessageDescriptor is a base class, not for direct use.

/// A message consists of one or more segments, where a
/// segment is a contiguous set of blocks within a single slab.
/// A MessageDescriptor points to a linked list of segments.
/// \see SendMessageDescriptor \see RecvMessageDescriptor
class MessageDescriptor {
  private:
	MessageSegment* seg;
	ClientImpl* cimpl;
	explicit MessageDescriptor(MessageSegment* s=0, ClientImpl* c=0)
		: seg(s), cimpl(c) {}
	explicit MessageDescriptor(SendMsgSegment* s, ClientImpl* c)
		: seg(reinterpret_cast<MessageSegment*>(s)), cimpl(c) {}
	explicit MessageDescriptor(RecvMsgSegment* s, ClientImpl* c)
		: seg(reinterpret_cast<MessageSegment*>(s)), cimpl(c) {}
	friend class SendMessageDescriptorRef;
	friend class SendMessageDescriptor;
	friend class RecvMessageDescriptor;
  public:
	/// Does this descriptor refer to a valid memory region?
	bool Valid(void) const { return seg && cimpl; }

	/// Does this descriptor refer to a single, contiguous region? (vs a multi-segment message).
	bool Contiguous(void) const;

	/// To how many segments does this descriptor refer?
	unsigned NumSegments(void) const;

	/// Size (in bytes) of the whole descriptor (summing possible multiple segments).
	uint32_t TotalSize(void) const;

	/// Get a standard iovec of pointers and length of the segments.

	/// If iovcnt is too small, return -iovcnt_needed.
	int GetIovec(struct iovec iov[], int iovcnt) const;
	
	/// Size (in bytes) of the first/current segment in the descriptor.
	uint32_t Size(void) const;

	/// blockID of the first/current segment in the descriptor.
	uint32_t BlockID(void) const;

	/// pointer to the first/current (opaque) MessageSegment in the descriptor.
	MessageSegment* Seg(void) const { return seg; }

	/// pointer to this descriptor's issuing (opaque) ClientImpl.
	ClientImpl* CImpl(void) const { return cimpl; }
};

/// SendMessageDescriptorRef is a helper class, not for direct use.

/// SendMessageDescriptorRef aids automatic type conversions for SendMessageDescriptor::operator=().
class SendMessageDescriptorRef : protected MessageDescriptor {
	friend class SendMessageDescriptor;
  public:
	/// Helper class, not for direct use.
	explicit SendMessageDescriptorRef(SendMsgSegment* s, ClientImpl* c)
		: MessageDescriptor(s,c) {}
};

/// Refers to one or more segments that will be used to send a single message.

/// It uses semantics like std::auto_ptr, taking ownership upon assignment.
/// The Reset/Release API closely follows auto_ptr.
/// Like auto_ptr, SendMessageDescriptors can't be stored in an STL container.
/// We would prefer unique_ptr<> semantics, but that requires C++11
/// (which may happen soon).
class SendMessageDescriptor : public MessageDescriptor {
  public:
	SendMessageDescriptor(void) {}
	
	/// Construct, *taking ownership* from another SendMessageDescriptor.
	SendMessageDescriptor(SendMessageDescriptor& desc)
		: MessageDescriptor(desc.Release(),desc.cimpl) {}

   ~SendMessageDescriptor(void);

	/// Assign, *taking ownership* from another SendMessageDescriptor.
	SendMessageDescriptor& operator=(SendMessageDescriptor& desc)
		{ Reset(desc.Release(),desc.cimpl); return *this; }

	/// Release a SendMsgSegment from a SendMessageDescriptor.
	/// (A SendMsgSegment *can* be put in an STL container, but smart/auto_ptr
	/// semantics are then lost).
	SendMsgSegment* Release(void);
	/// Reset, optionally filling with the underlying (opaque) types.
	/// (A SendMsgSegment can be put back into a SendMessageDescriptor).
	void Reset(SendMsgSegment* s=0, ClientImpl* c=0);

	/// Return a pointer to the first/current memory segment in the descriptor.
	void* Buf(void) const;
	/// Fill the descriptor with specified value (memset).
	void Fill(uint8_t val=0);

	/// Send the contents of this descriptor as a message via the issuing client. 
	int SendMessage(uint32_t msgID, uint32_t len);

	/// Helper to allow automatic conversions for assignment operator
	operator SendMessageDescriptorRef()
		{ return SendMessageDescriptorRef(Release(),cimpl); }
	/// Helper to allow automatic conversions for assignment operator
	SendMessageDescriptor(SendMessageDescriptorRef ref)
		: MessageDescriptor(ref.seg,ref.cimpl) {}
	/// Helper to allow automatic conversions for assignment operator
	SendMessageDescriptor& operator=(SendMessageDescriptorRef ref)
		{ Reset(reinterpret_cast<SendMsgSegment*>(ref.seg),ref.cimpl);
			return *this; }
};


/// Refers to one or more segments that have been received as a single message.

/// It uses semantics like std::shared_ptr, using reference counting, and
/// the resources it holds are freed when it goes out of scope.
class RecvMessageDescriptor : public MessageDescriptor {
  public:
	RecvMessageDescriptor(void) {}
	/// Copy with reference counting.
	RecvMessageDescriptor(const RecvMessageDescriptor& desc)
	: MessageDescriptor(desc.seg,desc.cimpl) { inc(); }
   ~RecvMessageDescriptor(void) { dec(); }
	/// Assignment with reference counting.
	RecvMessageDescriptor& operator=(const RecvMessageDescriptor& desc);

	/// Return a pointer to the first/current memory segment in the descriptor.
	const void* Buf() const;
	/// The messageID of the received message referred to by this descriptor.
	uint32_t MessageID(void) const;

	/// Advanced constructor using the underlying (opaque) types.
	explicit RecvMessageDescriptor(RecvMsgSegment* s, ClientImpl* c)
		: MessageDescriptor(s,c) {}

  private:
	void inc(void);
	void dec(void);
};

/// Copies a received message into a single contiguous, temporary buffer.

/// Allocate a temporary buffer with std::get_temporary_buffer<char>, and
/// throws if unsuccessful.
/// Copies entire received message into this single contiguous buffer.
/// Resulting buffer must be freed with std::return_temporary_buffer.
std::pair<char*,ptrdiff_t> GetTempBufferFromRMD(const RecvMessageDescriptor& desc);

} // namespace MCSB

#endif
