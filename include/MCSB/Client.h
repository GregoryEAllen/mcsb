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

/// \file Client.h
/// \brief A Client that receives messages reactively, via registration and callback.

#ifndef MCSB_Client_h
#define MCSB_Client_h
#pragma once

#include "MCSB/BaseClient.h"

namespace MCSB {

/// A Client that receives messages reactively, via registration and callback.
class Client : public BaseClient {
  public:
	/// Construct from ClientOptions, optionally deferring connection attempt.
	Client(const ClientOptions& co, bool connect=1): BaseClient(co,connect)
		{ Init(); }
	/// Construct from argc and argv.
	Client(int argc, char* const argv[]): BaseClient(argc,argv) { Init(); }
	virtual ~Client(void);

	/// Register a \RMD callback function to handle received messages with the specified msgID.
	void RegisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf, void* arg=0);
	/// Deregister a \RMD callback function to no longer receive messages with the specified msgID.
	bool DeregisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf, void* arg=0);

	/// Register a \RMD callback method to handle received messages with the specified msgID.
	template <class T, void (T::*method)(const RecvMessageDescriptor& desc)>
	void RegisterForMsgID(uint32_t msgID, T* object) {
		RegisterForMsgID(msgID, Thunk::MessageHandler<T,method>, reinterpret_cast<void*>(object));
	}
	/// Deregister a \RMD callback method to no longer receive messages with the specified msgID.
	template <class T, void (T::*method)(const RecvMessageDescriptor& desc)>
	bool DeregisterForMsgID(uint32_t msgID, T* object) {
		return DeregisterForMsgID(msgID, Thunk::MessageHandler<T,method>, reinterpret_cast<void*>(object));
	}

	/// Register a pointer/length callback method to handle received messages with the specified msgID.
	template <class T, void (T::*method)(uint32_t, const void*, uint32_t)>
	void RegisterForMsgID(uint32_t msgID, T* object) {
		RegisterForMsgID(msgID, Thunk::MessageHandlerP<T,method>, reinterpret_cast<void*>(object));
	}
	/// Deregister a pointer/length callback method to no longer receive messages with the specified msgID.
	template <class T, void (T::*method)(uint32_t, const void*, uint32_t)>
	bool DeregisterForMsgID(uint32_t msgID, T* object) {
		return DeregisterForMsgID(msgID, Thunk::MessageHandlerP<T,method>, reinterpret_cast<void*>(object));
	}

	/// Deregister to eliminate all callbacks and no longer receive any messages.
	void DeregisterForAllMsgs(void);

	/// Give up your main loop to repeatedly Poll() this client (rarely used).
	void Run(void); // Run() to give up my main loop, or Poll() inside my own
	/// Check the control socket, read and optionally call message handler callbacks.
	int Poll(float timeout=-1., bool serviceRecvMessages=1);
	/// Close the control socket connection to the Manager.
	void Close(void);
	/// Call any registered callbacks for the passed \RMD (advanced).
	void HandleRecvMessage(const RecvMessageDescriptor& rmd);

	/// This is needed to support SWIG/Python deregistering of Python methods.
	void* DeregisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf, void* arg,
		bool (*argsEqualFunc)(void* arg1, void* arg2) );

  private:
	void Init(void);
	int handlingRecvMessage;
	typedef std::multimap<uint32_t, std::pair<MessageHandlerFunction,void*> > MessageHandlerMultiMap;
	MessageHandlerMultiMap messageHandlers;
};

} // namespace MCSB

#endif
