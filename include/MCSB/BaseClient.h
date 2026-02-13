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

/// \file BaseClient.h
/// \brief A Client that sends and receives MCSB messages procedurally.

#ifndef MCSB_BaseClient_h
#define MCSB_BaseClient_h
#pragma once

#include "MCSB/ClientOptions.h"
#include "MCSB/MessageDescriptors.h"
#include "MCSB/ClientCallbacks.h"

#include <string>
#include <map>
#include <unistd.h>

namespace MCSB {

/// This single messageID is invalid/reserved.
enum { kInvalidMessageID = 0xFFFFFFFF };

/// A Client that sends and receives MCSB messages procedurally.
class BaseClient : public dbprinter {
  public:
	/// Construct from ClientOptions, optionally deferring connection attempt.
	BaseClient(const ClientOptions& co, bool connect=1): options(co)
		{ Init(connect); }
	/// Construct from argc and argv.
	BaseClient(int argc, char* const argv[]): options(argc,argv) { Init(1); }
	virtual ~BaseClient(void);

	/// Send a message on a msgID from a pointer and length (copying).
	int SendMessage(uint32_t msgID, const void* msg, uint32_t len);
	/// Send a message on a msgID from a standard iovec (copying).
	int SendMessage(uint32_t msgID, const struct iovec msgiov[], int iovcnt);

	/// Get a descriptor that filled in and later sent with SendMessage.
	SendMessageDescriptor GetSendMessageDescriptor(uint32_t len,
		bool contiguous=1, bool poll=1);
	/// Send a message on a msgID from a descriptor (zero-copy).
	int SendMessage(uint32_t msgID, SendMessageDescriptor& desc, uint32_t len);

	/// Check for a pending receive message.
	bool PendingRecvMessage(void);
	/// Get the next recv message as a descriptor.
	RecvMessageDescriptor GetRecvMessageDescriptor(void);

	/// Register to receive messages with the specified list of msgIDs.
	int RegisterMsgIDs(const uint32_t msgIDs[], int count);
	/// Deregister to no longer receive messages with the specified msgIDs.
	int DeregisterMsgIDs(const uint32_t msgIDs[], int count);

	/// Deregister to no longer receive any messages.
	void DeregisterAllMsgIDs(void);

	/// Check the control socket, read and handle if it is readable.
	int Poll(float timeout=-1.);

	// Below here are less frequently used advanced methods

	/// Accessor for the block size in bytes.
	uint32_t BlockSize(void) const;
	/// Accessor for the slab size in bytes.
	uint32_t SlabSize(void) const;
	/// The number of producer slabs allocated this client in bytes.
	uint32_t NumProducerSlabs(void) const;
	/// The number of consumer slabs allocated this client in bytes.
	uint32_t NumConsumerSlabs(void) const;
	/// The largest message that can be sent with this client in bytes.
	uint32_t MaxSendMessageSize(void) const;
	/// The largest message that can be received with this client in bytes.
	uint32_t MaxRecvMessageSize(void) const;
	/// Accessor for the file descriptor of the control socket.
	int FD(void) const;

	/// Accessor for this client's integer clientID, assigned by the Manager.
	int ClientID(void) const;
	/// Flush pending communication to the Manager (with a sequence token).
	int Flush(float timeout=-1.);
	/// Check whether the control socket is connected.
	bool Connected(void) const;
	/// Connect if not connected, and return whether connected.
	bool Connect(void);
	/// Close the control socket connection to the Manager.
	virtual void Close(void);
	/// Get a const reference to the internal ClientOptions object.
	const ClientOptions& GetClientOptions(void) const { return options; }

	/// The number of segments received but not yet processed (a queue of upcoming RecvMessages).
	unsigned PendingRecvSegments(void) const;
	/// The total number of segments received since this client connected to the Manager.
	uint64_t NumSegmentsRcvd(void) const;

	/// Send a sequence token to the Manager (for flushing).
	int SendSequenceToken(void);
	/// Return the number of sequence tokens sent but not received back.
	unsigned PendingSequenceTokens(void) const;

	/// Install a callback function that handles drop reports (arg is user data).
	void SetDropReportHandler(DropReportHandler func, void* arg=0);
	/// Install a callback method that handles drop reports.
	template <class T, void (T::*method)(uint32_t numSegs, uint32_t numBytes)>
	void SetDropReportHandler(T* object) {
		SetDropReportHandler(Thunk::DropReportHandler<T,method>, reinterpret_cast<void*>(object));
	}

	/// Install a callback function that handles connection events (arg is user data).
	void SetConnectionEventHandler(ConnectionEventHandler func, void* arg=0);
	/// Install a callback method that handles connection events.
	template <class T, void (T::*method)(int)>
	void SetConnectionEventHandler(T* object) {
		SetConnectionEventHandler(Thunk::ConnectionEventHandler<T,method>, reinterpret_cast<void*>(object));
	}

	/// Install a callback function that handles registration events (arg is user data).
	void SetRegistrationHandler(RegistrationHandler func, void* arg=0);
	/// Install a callback method that handles registration events.
	template <class T, void (T::*method)(bool reg, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count)>
	void SetRegistrationHandler(T* object) {
		SetRegistrationHandler(Thunk::RegistrationHandler<T,method>, reinterpret_cast<void*>(object));
	}

	/// Accessor for this client's integer groupID (if a groupID was requested).
	int GroupID(void) const;
	/// Request a groupID via string (advanced for routing message).
	int RequestGroupID(const char* groupStr, bool wait=1);

	/// Send an echo control message to the Manager (deferring a socket read event).
	int SendManagerEcho(void);

	/// Get a pointer to the internal (opaque) ClientImpl object.
	ClientImpl* GetClientImpl(void) { return cimpl; }
	/// Get a const pointer to the internal (opaque) ClientImpl object.
	const ClientImpl* GetClientImpl(void) const { return cimpl; }

  private:
	void Init(bool connect);
	std::string groupStr;
	int connecting;
	ClientImpl* cimpl;

	std::pair<DropReportHandler,void*> dropReportHandler;
	std::pair<ConnectionEventHandler,void*> connectionEventHandler;
	std::pair<RegistrationHandler,void*> registrationHandler;

	typedef std::map<uint32_t,uint32_t> MsgIdMap; // mid -> non-zero count
	MsgIdMap registeredMsgIDs;

  protected:
	/// The internal options used for this client.
	ClientOptions options;
};

/// Return the MCSB version string (also MCSB_VERSION).
const char* GetVersion(void);
/// Return the Mercurial repository version hash, or "" if not built in a repository.
const char* GetHgRevision(void);

} // namespace MCSB

#endif
