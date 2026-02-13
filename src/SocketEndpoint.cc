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

#include "MCSB/SocketEndpoint.h"
#include "MCSB/ShmDefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string>
#include <stdexcept>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <poll.h>


namespace MCSB {

// level for methods that should have been overridden
static int olvl = kNotice;

//-----------------------------------------------------------------------------
SocketEndpoint::SocketEndpoint(int fd, int vb, unsigned recvBufCap)
//-----------------------------------------------------------------------------
:	dbprinter(vb), sockFD(fd), recvBuf(recvBufCap), recvBufLen(0), sendFailed(0),
	validPeer(0), throwOnPeerDisconnect(1), groupID(0)
{
	if (recvBufCap<kMaxCtrlMsgSize)
		recvBuf.resize(kMaxCtrlMsgSize);

	SetSocketOptions();
	SendValidatePeer();
}

//-----------------------------------------------------------------------------
SocketEndpoint::~SocketEndpoint(void)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SetSocketOptions(void)
//-----------------------------------------------------------------------------
{
	int result = -1;

	if (sockFD<0)
		return result;

	// set non-blocking
	result = fcntl(sockFD, F_SETFL, O_NONBLOCK);
	if (result<0) {
		std::string err = "fcntl F_SETFL O_NONBLOCK error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

#ifdef SO_NOSIGPIPE
	// prefer to disallow SIGPIPE per socket, so we don't have to mask at process level
	// Linux doesn't have NOSIGPIPE, so we have to mask at process level or use MSG_NOSIGNAL
	int set = 1;
	result = setsockopt(sockFD, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
	if (result<0) {
		std::string err = "setsockopt SO_NOSIGPIPE error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	socklen_t len = sizeof(int);
	result = getsockopt(sockFD, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, &len);
	if (result<0) {
		dbprintf(kWarning,"### getsockopt SO_NOSIGPIPE error: %s\n", strerror(errno));
		return result;
	}
	if (len != sizeof(int) || !set)
		dbprintf(kWarning,"### SocketEndpoint::SetNoSigPipeOption failed: set %d, len %d\n", set, len);
#endif
	return result;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::Poll(float timeout)
//	if timeout>=0, call select with that time in seconds
//	returns the result of select or recv
//-----------------------------------------------------------------------------
{
	// if there is a non-zero timeout, call poll
	if (timeout) {
		int res = PollFD(sockFD, timeout);
		if (!res) return 0;
	}

	// read the socket into recvBuf
	unsigned bytesToRead = recvBuf.size() - recvBufLen;
	
	int res = recv(sockFD, &recvBuf[recvBufLen], bytesToRead, 0);
	if (res<0) {
		if (errno == EINTR) return 0;
		if (errno == EAGAIN) return 0;
		std::string err = "recv error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	} else if (!res) {
		// peer has closed its half side of the connection
		if (throwOnPeerDisconnect)
			throw std::runtime_error("socket peer closed connection");
		return res;
	}
	recvBufLen += res;

	// parse the pending recvBufBytes in recvBuf[]
	// call Parse until it returns 0
	while (recvBufLen>0) {
		int bytesParsed = Parse();
		if (!bytesParsed) break;
		// copy the unconsumed portion down to the base
		memmove(&recvBuf[0], &recvBuf[bytesParsed], recvBufLen-bytesParsed);
		recvBufLen -= bytesParsed;
	}

	return res;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::Parse(void)
// returns the number of bytes consumed
//-----------------------------------------------------------------------------
{
	uint32_t* blockIDs = (uint32_t*) &recvBuf[0];
	unsigned bytesToParse = recvBufLen;
	if (bytesToParse<sizeof(uint32_t)) return 0;

	while (1) {
		unsigned count = 0;

		// check for blockIDs/slabIDs (not ctrl msgs)
		while (bytesToParse-count*4>=4) {
			if (blockIDs[count]==CtrlMsgHdr::kToken) break;
			if (count)
				if ((blockIDs[0]&kSlabMask) != (blockIDs[count]&kSlabMask))
					break;
			count++;
		}
		if (count) {
			if (!validPeer) LocalHandleCtrlMsg(-1,0,0); // let him throw
			if (blockIDs[0]&kSlabMask) { // slabs
				// take out the masks
				for (unsigned i=0; i<count; i++)
					blockIDs[i] &= ~kSlabMask;
				HandleSlabIDs(blockIDs,count);
			} else { // blocks
				HandleBlockIDs(blockIDs,count);
			}
			blockIDs += count;
			bytesToParse -= count*4;
			continue;
		}
		
		// check for a ctrl msg
		if (bytesToParse<sizeof(CtrlMsgHdr)) break; // incomplete hdr
		assert(blockIDs[0]==CtrlMsgHdr::kToken);

		const CtrlMsgHdr* hdr = (const CtrlMsgHdr*) blockIDs;
		unsigned msgSize = sizeof(CtrlMsgHdr)+hdr->length;
		assert(!(hdr->length & (sizeof(uint32_t)-1))); // length is multiple of uint32_t

		if (bytesToParse<msgSize) break; // incomplete msg

		if (hdr->length>CtrlMsgHdr::kMaxPayloadSize) {
			std::string err = "protocol error: control message len>CtrlMsgHdr::kMaxPayloadSize";
			err += strerror(errno);
			throw std::runtime_error(err);
		}

		// call the handler on the received control message
		const char* ptr = (const char*)(hdr+1);
		LocalHandleCtrlMsg(hdr->msgID, ptr, hdr->length);
		blockIDs = (uint32_t*)(ptr + hdr->length);
		bytesToParse -= msgSize;
	}
	return recvBufLen - bytesToParse;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::PollFD(int fd, float timeout, short events)
//	poll the specified fd and return true if the event is true within timeout
//	timeout is in (fractional) seconds, timeout<0 is indefinite
//	if events==0, will use POLLIN (checking for readable)
//	e.g. events=POLLOUT will return true if fd is writable
//	returns true if the event is true (or error)
//-----------------------------------------------------------------------------
{
	if (!events)
		events = POLLIN;
	
	int timeout_ms = 0;
	if (timeout<0)
		timeout_ms = -1;
	else if (timeout)
		timeout_ms = int(timeout*1e3+0.999); // approx ceiling of ms

	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = events;
	fds[0].revents = 0;
	
	while (true) {
		int res = poll(fds,1,timeout_ms);
		if (res>0) {
			return fds[0].revents & events;
		} else if (res<0) {
			if (errno == EINTR) continue;
			perror("### poll error");
			return res;
		} else break; // timeout
	};
	return 0;
}

//-----------------------------------------------------------------------------
void SocketEndpoint::LocalHandleCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len)
//	handle the messages that are locally dealt with
//-----------------------------------------------------------------------------
{
	if (!validPeer && msgID!=kCtrlMsgID_ValidatePeer) {
		throw std::runtime_error("peer protocol error: Startup message required");
	}

	switch(msgID) {
	  case kCtrlMsgID_BlocksAndInfo: {
		unsigned unitSize = sizeof(uint32_t)+sizeof(BlockInfo);
		unsigned count = len/unitSize;
		if (len!=count*unitSize) {
			char str[100];
			sprintf(str,"error: kCtrlMsgID_BlocksAndInfo with len(%d) not a multiple of %d",len,unitSize);
			throw std::runtime_error(str);
		}
		const uint32_t* blocks = (uint32_t*)(ptr);
		const BlockInfo* info = (BlockInfo*)(blocks+count);
		HandleBlocksAndInfo(blocks,info,count);
	  } break;
	  case kCtrlMsgID_ManagerEcho: {
		HandleManagerEcho(ptr,len);
	  } break;
	  case kCtrlMsgID_SequenceToken: {
		if (len != sizeof(uint32_t)) {
			throw std::runtime_error("kCtrlMsgID_SequenceToken message invalid size");
		}
		uint32_t token = *((const uint32_t*)ptr);
		HandleSequenceToken(token);
	  } break;
	  case kCtrlMsgID_Registration+kRegType_RegisterList:
	  case kCtrlMsgID_Registration+kRegType_DeregisterList:
	  case kCtrlMsgID_Registration+kRegType_RegisterAllMsgs:
	  {
		const RegistrationMsgHdr* hdr = (const RegistrationMsgHdr*)ptr;
		const uint32_t* msgIDs = (const uint32_t*)(hdr+1);
		if (len<sizeof(RegistrationMsgHdr)) {
			throw std::runtime_error("error: payload smaller than RegistrationMsgHdr");
		}
		unsigned count = (len-sizeof(RegistrationMsgHdr))/sizeof(uint32_t);
		if (len!=sizeof(RegistrationMsgHdr)+count*sizeof(uint32_t)) {
			throw std::runtime_error("error: registration message invalid size");
		}
		unsigned type = msgID-kCtrlMsgID_Registration;
		HandleRegistration(type, hdr->clientID, hdr->groupID, msgIDs, count);
	  } break;
	  case kCtrlMsgID_DropReport: {
		if (len != 2*sizeof(uint32_t)) {
			throw std::runtime_error("kCtrlMsgID_DropReport message invalid size");
		}
		uint32_t* vals = ((uint32_t*)ptr);
		HandleDropReport(vals[0],vals[1]);
	  } break;
	  case kCtrlMsgID_DropReportAck: {
		if (len) {
			throw std::runtime_error("kCtrlMsgID_DropReportAck message invalid size");
		}
		HandleDropReportAck();
	  } break;
	  case kCtrlMsgID_CtrlString: {
		uint32_t which = *((const uint32_t*)ptr);
		const char* str = (const char*)ptr+sizeof(uint32_t);
		HandleCtrlString(which, str);
	  } break;
	  case kCtrlMsgID_ValidatePeer: {
		uint32_t* vals = ((uint32_t*)ptr);
		if (len != 2*sizeof(uint32_t)) {
			throw std::runtime_error("error: kCtrlMsgID_ValidatePeer message invalid size");
		}
		HandleValidatePeer(vals[0],vals[1]);
	  } break;
	  case kCtrlMsgID_NumSlabs: {
		if (len != 2*sizeof(uint32_t)) {
			throw std::runtime_error("kCtrlMsgID_NumSlabs incorrect size");
		}
		const uint32_t* slabs = (const uint32_t*)ptr;
		HandleNumSlabs(slabs[0],slabs[1]);
	  } break;
	  case kCtrlMsgID_ClientID: {
		int32_t id = *((int32_t*)ptr);
		if (len!=sizeof(id)) {
			throw std::runtime_error("kCtrlMsgID_ClientID incorrect size");
		}
		HandleClientID(id);
	  } break;
	  case kCtrlMsgID_GroupID: {
		int32_t id = *((int32_t*)ptr);
		if (len!=sizeof(id)) {
			throw std::runtime_error("kCtrlMsgID_GroupID incorrect size");
		}
		HandleGroupID(id);
	  } break;
	  case kCtrlMsgID_ClientPID: {
		if (len != sizeof(uint32_t)) {
			throw std::runtime_error("kCtrlMsgID_ClientID incorrect size");
		}
		uint32_t pid = *((const uint32_t*)ptr);
		HandleClientPID(pid);
	  } break;
	  case kCtrlMsgID_RegistrationsWanted: {
		if (len != 4) {
			throw std::runtime_error("kCtrlMsgID_RegistrationsWanted incorrect size");
		}
		const char* val = (const char*)ptr;
		HandleRegistrationsWanted(val[0], val[1]);
	  } break;
	  case kCtrlMsgID_ProdNiceLevel: {
		if (len != 4) {
			throw std::runtime_error("kCtrlMsgID_ProdNiceLevel incorrect size");
		}
		HandleProdNiceLevel(*((const int32_t*)ptr));
	  } break;

	  default:
		HandleCtrlMsg(msgID,ptr,len);
	}
}


//-----------------------------------------------------------------------------
int SocketEndpoint::SendCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len)
//-----------------------------------------------------------------------------
{
	const void* ptrs[2] = { ptr, 0 };
	return SendCtrlMsg(msgID,ptrs,&len);
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendCtrlMsg(uint16_t msgID, const void* ptrs[], uint16_t lens[])
// returns number of bytes written
// using sendmsg for its iovec interface
//-----------------------------------------------------------------------------
{
	if (sendFailed)
		throw std::runtime_error("send error: refusing after send failure");

	const unsigned maxArgs = IOV_MAX-1;

	struct msghdr msgh;
	memset(&msgh,0,sizeof(struct msghdr));

	struct iovec vec[maxArgs+1];
	msgh.msg_iov = vec;
	
	unsigned numArgs = 0;
	unsigned totalLen = 0;
	for (; numArgs<maxArgs && ptrs[numArgs]; numArgs++) {
		totalLen += lens[numArgs];
		vec[numArgs+1].iov_base = (void*) ptrs[numArgs];
		vec[numArgs+1].iov_len = lens[numArgs];
	}
 	if (totalLen>CtrlMsgHdr::kMaxPayloadSize) {
		throw std::runtime_error("protocol error: can't send control message larger than kMaxCtrlMsgSize");
	}

	// prepend CtrlMsgHdr 
	CtrlMsgHdr hdr(msgID,totalLen);
	vec[0].iov_base = (void*) &hdr;
	vec[0].iov_len = sizeof(hdr);
	msgh.msg_iovlen = numArgs+1;
	totalLen += sizeof(hdr);

	int flags = 0;
	#ifdef MSG_NOSIGNAL
		flags = MSG_NOSIGNAL;	// we don't want SIGPIPE (Linux)
	#endif

	unsigned totalSent = 0;

	while (totalSent<totalLen) {
		int sent = sendmsg(sockFD,&msgh,flags);
		if (sent<0) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				if (HandleSendWouldBlock())
					continue;
			sendFailed = 1;
			std::string err = "sendmsg error: ";
			err += strerror(errno);
			throw std::runtime_error(err);
		}
		totalSent += sent;
		if (totalSent==totalLen) break;
		// advance the iovec and msghdr by sent
		// FIXME: this needs testing
		while (sent-msgh.msg_iov->iov_len>0) {
			sent -= msgh.msg_iov->iov_len;
			msgh.msg_iov++;
			msgh.msg_iovlen--;
		}
		if (sent>0) {
			msgh.msg_iov->iov_base = (void*)((char*)(msgh.msg_iov->iov_base) + sent);
			msgh.msg_iov->iov_len -= sent;
		}
	}

	return totalSent;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendBlockIDs(const uint32_t blockIDs[], unsigned count)
// returns number of bytes written
//-----------------------------------------------------------------------------
{
	if (sendFailed)
		throw std::runtime_error("send error: refusing after send failure");
	
	int flags = 0;
	#ifdef MSG_NOSIGNAL
		flags = MSG_NOSIGNAL;	// we don't want SIGPIPE (Linux)
	#endif

	unsigned len = sizeof(uint32_t)*count;
	const char* pay = (const char*)blockIDs;
	unsigned totalSent = 0;
	
	while (totalSent<len) {
		int sent = send(sockFD,pay,len,flags);
		if (sent<0) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				if (HandleSendWouldBlock())
					continue;
			sendFailed = 1;
			std::string err = "send error: ";
			err += strerror(errno);
			throw std::runtime_error(err);
		}
		totalSent += sent;
		len -= sent;
		pay += sent;
	}

	return totalSent;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendSlabIDs(const uint32_t slabIDs[], unsigned count)
// returns number of bytes written
//-----------------------------------------------------------------------------
{
	// copy them, set kSlabMask, and send
	uint32_t sids[count];
	for (unsigned i=0; i<count; i++)
		sids[i] = slabIDs[i] | kSlabMask;
	return SendBlockIDs(sids,count);
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendCtrlString(uint32_t which, const char* str)
//-----------------------------------------------------------------------------
{
	uint16_t len = strlen(str)+1;	// include the terminating null
	
	const void* ptrs[4]    = { &which, str, 0, 0 };
	uint16_t lens[4] = { sizeof(uint32_t), len, 0, 0 };
	char zeros[4] = { 0, 0, 0, 0 };
	
	// zero pad up to a multiple of sizeof(uint32_t) bytes
	int pad = -len & (sizeof(uint32_t)-1);
	if (pad) {
		ptrs[2] = zeros;
		lens[2] = pad;
	}
	
	return SendCtrlMsg(kCtrlMsgID_CtrlString,ptrs,lens);
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendBlocksAndInfo(const uint32_t blockNums[],
	const BlockInfo blockInfo[], unsigned count)
//-----------------------------------------------------------------------------
{
	unsigned maxPerSend = CtrlMsgHdr::kMaxPayloadSize/(sizeof(uint32_t)+sizeof(BlockInfo));
	unsigned totalSent = 0;

	while (count) {
		unsigned toSend = count;
		if (toSend>maxPerSend)
			toSend = maxPerSend;
		const void* ptrs[3] = { blockNums, blockInfo, 0 };
		uint16_t lens[3]    = { (uint16_t)(sizeof(uint32_t)*toSend), (uint16_t)(sizeof(BlockInfo)*toSend), 0 };
		totalSent += SendCtrlMsg(kCtrlMsgID_BlocksAndInfo,ptrs,lens);
		if (totalSent==count) break;
		count -= toSend;
		blockNums += toSend;
		blockInfo += toSend;
	}
	return totalSent;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendDropReport(uint32_t segs, uint32_t bytes)
//-----------------------------------------------------------------------------
{
	uint32_t ints[2] = { segs, bytes };
	return SendCtrlMsg(kCtrlMsgID_DropReport,(void*)ints,sizeof(ints));
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendNumSlabs(uint32_t prodSlabs, uint32_t consSlabs)
//-----------------------------------------------------------------------------
{
	uint32_t ints[2] = { prodSlabs, consSlabs };
	return SendCtrlMsg(kCtrlMsgID_NumSlabs,(void*)ints,sizeof(ints));
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendClientID(int16_t cid)
//-----------------------------------------------------------------------------
{
	int32_t id = cid;
	return SendCtrlMsg(kCtrlMsgID_ClientID,(void*)&id,sizeof(id));
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendGroupID(int16_t gid, bool setLocal)
//-----------------------------------------------------------------------------
{
	if (setLocal) {
		groupID = gid;
	}
	int32_t id = gid;
	return SendCtrlMsg(kCtrlMsgID_GroupID,(void*)&id,sizeof(id));
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendRegistration(uint32_t type, int16_t cltID, int16_t grpID, const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	if (type>kRegType_RegisterAllMsgs) {
		throw std::runtime_error("SendRegistration error: invalid value for type");
	}

	RegistrationMsg message(cltID,grpID);

	int ret = 0;
	while (count) {
		unsigned sendingCount = count;
		if (sendingCount>RegistrationMsg::kMaxMsgIDs)
			sendingCount = RegistrationMsg::kMaxMsgIDs;
		
		memcpy(message.msgIDs,msgIDs,sendingCount*sizeof(uint32_t));
		unsigned len = sizeof(RegistrationMsgHdr) + sendingCount*sizeof(uint32_t);

		ret += SendCtrlMsg(kCtrlMsgID_Registration+type,(void*)&message,len);

		count -= sendingCount;
		msgIDs += sendingCount;
	}
	
	return ret;
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendValidatePeer(void)
//-----------------------------------------------------------------------------
{
	uint32_t vals[2] = { kProtocolMagic, kProtocolVersion };
	return SendCtrlMsg(kCtrlMsgID_ValidatePeer,vals,sizeof(vals));
}

//-----------------------------------------------------------------------------
int SocketEndpoint::SendRegistrationsWanted(bool wanted)
//-----------------------------------------------------------------------------
{
	char val[4] = { wanted, wanted, 0, 0 };
	return SendCtrlMsg(kCtrlMsgID_RegistrationsWanted, val, sizeof(val));
}

//-----------------------------------------------------------------------------
unsigned SocketEndpoint::GetSockBufSize(bool send)
//-----------------------------------------------------------------------------
{
	unsigned size;
	socklen_t len = sizeof(size);
	int optname = send ? SO_SNDBUF : SO_RCVBUF;
	if (getsockopt(sockFD,SOL_SOCKET,optname,&size,&len)<0) {
		std::string err = "getsockopt error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	if (len != sizeof(size)) {
		dbprintf(kError,"### len %u, sizeof(size) %u\n", len, sizeof(size));
		throw std::runtime_error("getsockopt size mismatch");
	}
#ifdef __linux__
	// linux returns a doubled value (for internal bookkeeping)
	// see man 7 socket
	size /= 2;
#endif
	return size;
}

//-----------------------------------------------------------------------------
void SocketEndpoint::SetSockBufSize(unsigned size, bool send)
//-----------------------------------------------------------------------------
{
	int optname = send ? SO_SNDBUF : SO_RCVBUF;
	if (setsockopt(sockFD,SOL_SOCKET,optname,&size,sizeof(unsigned))<0) {
		std::string err = "setsockopt error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleValidatePeer(uint32_t magic, uint32_t version)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"- SocketEndpoint::HandleValidatePeer(%d,%d)\n", magic,version);
	if (magic != kProtocolMagic) {
		char str[100];
		sprintf(str,"invalid kProtocolMagic: 0x%08X", magic);
		throw std::runtime_error(str);
	}
	if (version != kProtocolVersion) {
		char str[100];
		sprintf(str,"invalid kProtocolVersion: %d (expect %d)", version, kProtocolVersion);
		throw std::runtime_error(str);
	}
	validPeer = 1;
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleClientID(int16_t id)
//-----------------------------------------------------------------------------
{
	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__);
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleGroupID(int16_t id)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"- SocketEndpoint::HandleGroupID(%d)\n", id);
	groupID = id;
	dbprintf(kInfo,"- groupID set to %d\n", groupID);
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleBlockIDs(const uint32_t blockIDs[], unsigned count)
//	to be overridden
//-----------------------------------------------------------------------------
{
	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__);
	dbprintf(olvl,"- %u blocks [ ", count);
	for (unsigned i=0; i<count; i++) {
		dbprintf(olvl,"%u ", blockIDs[i]);
	}
	dbprintf(olvl,"]\n");
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleSlabIDs(const uint32_t slabIDs[], unsigned count)
//	to be overridden
//-----------------------------------------------------------------------------
{
	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__);
	dbprintf(olvl,"- %u slabs [ ", count);
	for (unsigned i=0; i<count; i++) {
		dbprintf(olvl,"%u ", slabIDs[i]);
	}
	dbprintf(olvl,"]\n");
}

//-----------------------------------------------------------------------------
void SocketEndpoint::HandleRegistration(uint32_t type, int16_t cid, int16_t gid, const uint32_t msgIDs[], unsigned count)
//	to be overridden
//-----------------------------------------------------------------------------
{
	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__);
	dbprintf(olvl,"type=%u, clientID=%d, groupID=%d\n", type, cid, gid);
	dbprintf(olvl,"- %u msgIDs [ ", count);
	for (unsigned i=0; i<count; i++) {
		dbprintf(olvl,"%u ", msgIDs[i]);
	}
	dbprintf(olvl,"]\n");
}

//-----------------------------------------------------------------------------
int SocketEndpoint::HandleSendWouldBlock(void)
//	return true to try again, or false to fail
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	int result = 0;
	while (!result) {
		result = PollFD(sockFD, -1, POLLOUT); // wait indefinitely until writable
	}
	return !!result;
}

//-----------------------------------------------------------------------------
//	methods to be overridden
//-----------------------------------------------------------------------------
void SocketEndpoint::HandleBlocksAndInfo(const uint32_t blockIDs[], const BlockInfo info[], unsigned count)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleManagerEcho(const void* ptr, uint16_t len)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleCtrlString(uint32_t which, const char* str)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleDropReport(uint32_t segs, uint32_t bytes)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleDropReportAck(void)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleNumSlabs(uint32_t prodSlabs, uint32_t consSlabs)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleSequenceToken(uint32_t token)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleClientPID(int32_t pid)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleRegistrationsWanted(bool wantNew, bool wantCurrent)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
void SocketEndpoint::HandleProdNiceLevel(int32_t lvl)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
//-----------------------------------------------------------------------------
void SocketEndpoint::HandleCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len)
{	dbprintf(olvl,"# %s\n", __PRETTY_FUNCTION__); }
//-----------------------------------------------------------------------------

} // namespace MCSB
