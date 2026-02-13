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

#include "MCSB/ClientImpl.h"
#include "MCSB/ClientOptions.h"
#include "MCSB/crc32c.h"
#include "MCSB/uptimer.h"
#include "MCSB/CCIHeader.h"

#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
ClientImpl::ClientImpl(int fd, const ClientOptions& opts_)
//-----------------------------------------------------------------------------
:	SocketEndpoint(fd,opts_.verbosity,kMaxCtrlMsgSize),
	opts(opts_), clientID(-1),
	numProdSlabs(0), numProdSlabsRqstd(0), numConsSlabs(0), numConsSlabsRqstd(0),
	sequenceTokenSent(0), sequenceTokenRcvd(0), dropReportHandler(0,0),
	connectionEventHandler(0,0), registrationHandler(0,0),
	crcErrors(0), sendCallingPoll(0)
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	SendClientPID(getpid());
	SendCtrlString(kCtrlString_ClientName, opts.clientName.c_str());
	if (opts.producerNiceLevel) {
		SendProdNiceLevel(opts.producerNiceLevel);
	}
}

//-----------------------------------------------------------------------------
ClientImpl::~ClientImpl(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	if (close(FD())) {
		dbprintf(kError, "#-- close error: %s\n", strerror(errno));
	}
}

//-----------------------------------------------------------------------------
int ClientImpl::SendRegistration(uint32_t type, const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	while(clientID<0) Poll();
	return SocketEndpoint::SendRegistration(type,clientID,groupID,msgIDs,count);
}

//-----------------------------------------------------------------------------
int ClientImpl::SendRetiredSlabs(void)
//-----------------------------------------------------------------------------
{
	unsigned numRetiredSlabs = sendMgr.NumRetiredSlabs();
	if (numRetiredSlabs) {
		uint32_t retiredSlabs[numRetiredSlabs];
		sendMgr.GetRetiredSlabs(retiredSlabs,numRetiredSlabs);
		return SendSlabIDs(retiredSlabs,numRetiredSlabs);
	}
	return 0;
}

//-----------------------------------------------------------------------------
int ClientImpl::SendRetiredSegments(void)
//-----------------------------------------------------------------------------
{
	unsigned numRetiredSegments = recvMgr.NumRetiredSegments();
	if (numRetiredSegments) {
		uint32_t blockIDs[numRetiredSegments];
		recvMgr.GetRetiredSegments(blockIDs,numRetiredSegments);
		return SendBlockIDs(blockIDs,numRetiredSegments);
	}
	return 0;
}

//-----------------------------------------------------------------------------
int ClientImpl::SendMessage(uint32_t msgID, const void* msg, uint32_t len)
// a copying interface
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	bool contiguous = 0;
	SendMsgDesc desc = GetSendMsgDesc(len,contiguous);

	const SendMsgSegment* seg = desc;
	uint32_t bytesLeft = len;
	const char* src = (const char*)msg;
	while (bytesLeft) {
		void* dst = seg->Buf();
		uint32_t segBytes = (bytesLeft<=seg->Size()) ? bytesLeft : seg->Size();
		memcpy(dst,src,segBytes);
		bytesLeft -= segBytes;
		src += segBytes;
		seg = seg->Next();
	}

	return SendMessage(msgID, desc, len);
}

//-----------------------------------------------------------------------------
int ClientImpl::SendMessage(uint32_t msgID, const struct iovec iov[], int iovcnt)
// a copying interface
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	size_t len = 0;
	for (int i=0; i<iovcnt; i++) {
		len += iov[i].iov_len;
	}

	bool contiguous = 0;
	SendMsgDesc desc = GetSendMsgDesc(len,contiguous);

	const SendMsgSegment* seg = desc;
	uint32_t bytesLeft = len;
	uint32_t iovBytesLeft = 0;
	uint32_t segBytesLeft = 0;
	const char* src = 0;
	char* dst = 0;
	while (bytesLeft) {
		if (!iovBytesLeft) {
			src = (const char*)iov->iov_base;
			iovBytesLeft = iov->iov_len;
			iov++;
			continue;
		}
		if (!segBytesLeft) {
			dst = (char*)seg->Buf();
			segBytesLeft = seg->Size();
			seg = seg->Next();
			continue;
		}
		uint32_t bytes = bytesLeft;
		if (bytes>iovBytesLeft) bytes = iovBytesLeft;
		if (bytes>segBytesLeft) bytes = segBytesLeft;
		memcpy(dst,src,bytes);
		bytesLeft -= bytes;
		iovBytesLeft -= bytes;
		segBytesLeft -= bytes;
		src += bytes;
		dst += bytes;
	}

	return SendMessage(msgID, desc, len);
}

//-----------------------------------------------------------------------------
ClientImpl::SendMsgDesc
	ClientImpl::GetSendMsgDesc(uint32_t len, bool contiguous, bool poll)
// get a descriptor where we can place the message data for sending
// contiguous: only return a contiguous descriptor
// poll: call Poll (and block) if waiting on the manager
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	if (sendFailed) {
		throw std::runtime_error("GetSendMsgDesc after sendFailed");
	}
	
	SendMsgSegment* seg = 0;
	while (!seg) {
		seg = sendMgr.GetMessageDescriptor(len,contiguous);
		if (seg) break;
		// post-mortem
		if (clientID<0 || numProdSlabs<1) {
			// we're not initialized
			if (!poll) break;
			sendCallingPoll++;
			if (connectionEventHandler.first)
				(*connectionEventHandler.first)(kPoll,connectionEventHandler.second);
			Poll();
			continue;
		}
		if (numProdSlabs>sendMgr.SlabsHeld()) {
			// we're waiting for slabs from the manager
			if (!poll) break;
			sendCallingPoll++;
			if (connectionEventHandler.first)
				(*connectionEventHandler.first)(kPoll,connectionEventHandler.second);
			Poll();
			continue;
		} else {
			// no help is coming, squeeze and ask for help
			unsigned numRetiredSlabs = sendMgr.NumRetiredSlabs();
			if (!numRetiredSlabs) {
				sendMgr.FlushWorkingSlabs();
				numRetiredSlabs = sendMgr.NumRetiredSlabs();
			}
			if (numRetiredSlabs) {
				SendRetiredSlabs(); // send for help
				continue;
			}
			// we have all the slabs we're going to get, but still no alloc
			char str[256];
			sprintf(str,"ClientImpl::GetSendMsgDesc(%u,%d) failed", len, contiguous);
			uint32_t maxSendMessageSize = numProdSlabs * shm.SlabSize();
			if (len>maxSendMessageSize) {
				dbprintf(kWarning,"# max message size is %u\n", maxSendMessageSize);
			}
			dbprintf(kInfo,"# slabs { free:%u, working:%u, full:%u, retired:%u }\n",
				sendMgr.NumFreeSlabs(), sendMgr.NumWorkingSlabs(),
				sendMgr.NumFullSlabs(), sendMgr.NumRetiredSlabs());
			throw std::runtime_error(str);
		}
	}
	return seg;
}

//-----------------------------------------------------------------------------
int ClientImpl::SendMessage(uint32_t msgID, SendMsgDesc desc, uint32_t len)
// send the zero-copy message and release the descriptor
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	ClientSendManager::DescriptorReleaser releaser(desc,sendMgr);
	const SendMsgSegment* seg = desc;
	if (!seg) {
		throw std::runtime_error("null descriptor passed to SendMessage");
	}
	unsigned numSegments = seg->NumSegments();
	uint32_t  blockIDs[numSegments];
	blockInfo.resize(numSegments);
	uint32_t bytesLeft = len;
	unsigned segIdx = 0;
	while (seg) {
		blockIDs[segIdx] = seg->BlockID();
		bzero(&blockInfo[segIdx], sizeof(BlockInfo));
		blockInfo[segIdx].messageID = msgID;
		uint32_t segBytes = (bytesLeft<=seg->Size()) ? bytesLeft : seg->Size();
		blockInfo[segIdx].size = segBytes;
		blockInfo[segIdx].segmentNumber = segIdx;
		if (opts.crcPolicy & ClientOptions::eSetCrcs)
			blockInfo[segIdx].crc32c = crc32c(seg->Buf(),segBytes);
		seg = seg->Next();
		segIdx++;
		bytesLeft -= segBytes;
		if (!bytesLeft) break;
	}
	if (bytesLeft) {
		// the message didn't fit in the descriptor's segments!
		char str[256];
		seg = desc;
		size_t totalSize = seg->TotalSize();
		sprintf(str,"ClientImpl::SendMessage len %lu > descriptor len %lu",
			(unsigned long)len, (unsigned long)totalSize);
		throw std::runtime_error(str);
	}
	// loop over and fill out the number of segments actually used
	unsigned segmentsUsed = segIdx;
	double sendTime = 0; // FIXME
	for (unsigned segIdx=0; segIdx<segmentsUsed; segIdx++) {
		blockInfo[segIdx].numSegments = segmentsUsed;
		blockInfo[segIdx].sendTime = sendTime;
	}
	int result = SendBlocksAndInfo(blockIDs,&blockInfo[0],segmentsUsed);
	SendRetiredSlabs();
	//if (result>0) sendMsgCount++; FIXME?
	return result>0 ? len : result; // return message length if successful
}

//-----------------------------------------------------------------------------
int ClientImpl::SendCCI(uint32_t cciMsgID, const void* msg, uint32_t len)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	CCIHeader hdr(cciMsgID,clientID);
	struct iovec iov[2];
	iov[0].iov_base = &hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = (void*)msg; // iovec is not const correct
	iov[1].iov_len = len;
	return SendMessage(kCCIMessageID,&iov,2);
}

//-----------------------------------------------------------------------------
int16_t ClientImpl::RequestGroupID(const char* groupStr, bool wait)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	groupID = -1;
	SendCtrlString(kCtrlString_GroupID, groupStr);
	if (!wait) return GroupID();
	uptimer upt;
	while (upt.uptime()<5) { // time out in 5 seconds
		if (connectionEventHandler.first)
			(*connectionEventHandler.first)(kPoll,connectionEventHandler.second);
		Poll(.2);
		if (GroupID()>=0) return GroupID();
	}
	throw std::runtime_error("error: timed out waiting for groupID from manager");
}

//-----------------------------------------------------------------------------
void ClientImpl::ReleaseSendMsgDesc(SendMsgDesc desc)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	sendMgr.ReleaseMessageDescriptor(desc);
	// this may have retired send slabs
	SendRetiredSlabs();
}

//-----------------------------------------------------------------------------
bool ClientImpl::PendingRecvMessage(void)
//-----------------------------------------------------------------------------
{
	bool result = recvMgr.PendingMessage();
	// this retired any invalid segments (checksum or multi-seg problems)
	SendRetiredSegments();
	return result;
}

//-----------------------------------------------------------------------------
ClientImpl::RecvMsgDesc ClientImpl::GetRecvMsgDesc(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	RecvMsgDesc desc;
	while (true) {
		desc = recvMgr.GetMessageDescriptor();
		if (desc && (opts.crcPolicy & ClientOptions::eVerifyCrcs) &&
			!desc->ValidCRC(1)) {
			crcErrors++;
			ReleaseRecvMsgDesc(desc);
		} else break;
	}

	// this may have retired segments
	if (recvMgr.NumRetiredSegments()) SendRetiredSegments();
	return desc;
}

//-----------------------------------------------------------------------------
void ClientImpl::ReleaseRecvMsgDesc(RecvMsgDesc desc)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	recvMgr.ReleaseMessageDescriptor(desc);
	// this retired segments
	SendRetiredSegments();
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleClientID(int16_t id)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	clientID = id;
	dbprintf(kInfo,"- clientID set to %d\n", clientID);
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleCtrlString(uint32_t which, const char* str)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	switch (which) {
		case kCtrlString_ShmName: {
			shm.NameFormat(str);
			unsigned numBuffers = shm.MapBuffers();
			dbprintf(kInfo,"- client mapped %u buffer(s)\n", numBuffers);
			ComputeAndSendNumSlabs();
			} break;
		default:
			dbprintf(kNotice,"# unknown CtrlString type %u in %s\n", which, __PRETTY_FUNCTION__);
	}
}

//-----------------------------------------------------------------------------
int ClientImpl::ComputeAndSendNumSlabs(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	uint32_t blockSize = shm.BlockSize();
	uint32_t slabSize = shm.SlabSize();
	uint32_t prod = (opts.minProducerBytes+slabSize-1)/slabSize;
	uint32_t cons = (opts.minConsumerBytes+slabSize-1)/slabSize;
	if (prod<opts.minProducerSlabs) prod = opts.minProducerSlabs;
	if (cons<opts.minConsumerSlabs) cons = opts.minConsumerSlabs;
	if (prod<1) prod = 1;
	if (cons<1) cons = 1;

	sendMgr.SetSizeParams(blockSize,slabSize);
	sendMgr.SetNumTotalSlabs(shm.NumBuffers()*shm.SlabsPerBuffer());

	if (prod==numProdSlabs && cons==numConsSlabs)
		return 0;

	numProdSlabsRqstd = prod;
	numConsSlabsRqstd = cons;
	return SendNumSlabs(prod,cons);
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleNumSlabs(uint32_t prodSlabs, uint32_t consSlabs)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	numProdSlabs = prodSlabs;
	numConsSlabs = consSlabs;

	sendMgr.SetNumTotalSlabs(shm.NumBuffers()*shm.SlabsPerBuffer());
	recvMgr.NumConsSlabs(numConsSlabs);

	// FIXME: set socket buffer sizes?
	SetSendSockBufSize(1024*1024);
	SetRecvSockBufSize(1024*1024);
	dbprintf(kInfo,"- client[%d]: sendBuf %u, recvBuf %u\n",
		clientID, GetSendSockBufSize(), GetRecvSockBufSize());

	char err[256];
	if (numProdSlabsRqstd!=numProdSlabs || numConsSlabsRqstd!=numConsSlabs) {
		sprintf(err,"Manager denied request for numProdSlabs: %u, numConsSlabs: %u", numProdSlabsRqstd, numConsSlabsRqstd);
		throw std::runtime_error(err);
	}
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleDropReport(uint32_t segs, uint32_t bytes)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	if (dropReportHandler.first) {
		(*dropReportHandler.first)(segs,bytes,dropReportHandler.second);
	} else {
		dbprintf(kNotice,"# DropReport: { segments: %u, bytes: %u }\n", segs, bytes);
	}
	SendDropReportAck();
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleSlabIDs(const uint32_t slabIDs[], unsigned count)
// these are slabs to fill with send segments/messages
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	void* slabPtrs[count];
	for (unsigned i=0; i<count; i++) {
		uint32_t blockID = slabIDs[i]*shm.BlocksPerSlab();
		slabPtrs[i] = shm.GetWriteableBlockPtr(blockID);
	}
	sendMgr.AddFreeSlabs(slabIDs,slabPtrs,count);
	SendRetiredSlabs();
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleBlockIDs(const uint32_t blockIDs[], unsigned count)
// these are segments/messages to be received
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	const void* blockPtrs[count];
	const BlockInfo* blockInfoPtrs[count];
	for (unsigned i=0; i<count; i++) {
		ldiv_t result = ldiv(blockIDs[i],shm.BlocksPerBuffer());
		assert(result.quot<shm.NumBuffers());
		blockPtrs[i] = shm.GetBlockPtr(result.quot,result.rem);
		blockInfoPtrs[i] = shm.GetBlockInfo(result.quot,result.rem);
	}
	recvMgr.AddSegments(blockIDs,blockPtrs,blockInfoPtrs,count);
}

//-----------------------------------------------------------------------------
void ClientImpl::HandleRegistration(uint32_t type, int16_t clientID,
	int16_t groupID, const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	if (registrationHandler.first) {
		if (type!=kRegType_RegisterList && type!=kRegType_DeregisterList)
			return;
		bool reg = (type==kRegType_RegisterList);
		(*registrationHandler.first)(reg,clientID,groupID,msgIDs,count,
			registrationHandler.second);
	}
}

//-----------------------------------------------------------------------------
unsigned ClientImpl::PendingSequenceTokens(void) const
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	if (sequenceTokenSent>=sequenceTokenRcvd)
		return sequenceTokenSent-sequenceTokenRcvd;
	else
		return 0xffffffff - sequenceTokenRcvd + sequenceTokenSent + 1;
}

//-----------------------------------------------------------------------------
void ClientImpl::PrintState(void) const
//-----------------------------------------------------------------------------
{
	int lvl = kInfo;
	dbprintf(lvl,"---\n");
	dbprintf(lvl,"recvManagerState:\n");
	if (Verbosity()>=lvl)
		recvMgr.PrintState("  ");
	dbprintf(lvl,"...\n");
}

} // namespace MCSB
