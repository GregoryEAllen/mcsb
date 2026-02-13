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

/// \file ClientCallbacks.h
/// \brief Definitions of callback function types used in Client and BaseClient.
///
/// Inline template thunks are also defined in this file, but excluded from Doxygen.

#ifndef MCSB_ClientCallbacks_h
#define MCSB_ClientCallbacks_h
#pragma once

#include "MCSB/MessageDescriptors.h"

#include <memory>
#include <cassert>

namespace MCSB {

/// Signature of a callback function to handle recv messages (arg is user data).
typedef void (*MessageHandlerFunction)(const RecvMessageDescriptor& desc, void* arg);

/// Signature of a callback function to handle dropped segment reports (arg is user data).
typedef void (*DropReportHandler)(uint32_t numSegments, uint32_t numBytes, void* arg);

/// Signature of a callback function to handle connection events (arg is user data).
typedef void (*ConnectionEventHandler)(int type, void* arg);

/// Different types of connection events that may be called by ConnectionEventHandler.
enum ConnectionEventTypes {
	kDisconnection = 0, ///< The connection is lost, memory will be unmapped, and all allocated descriptors will become invalid
	kConnection,        ///< Client has successfully opened socket to Manager, and is about to construct ClientImpl
	kPoll               ///< Client will poll ClientImpl. (This allows a shared event loop).
};

/// \brief Signature of a callback function to handle registration events (arg is user data).
/// If reg: this is a registration, else: this is a de-registration.
typedef void (*RegistrationHandler)(bool reg, int16_t clientID, int16_t groupID,
	const uint32_t msgIDs[], unsigned count, void* arg);


//-----------------------------------------------------------------------------
/// Thunk implementations, internal details that are excluded from Doxygen.
namespace Thunk {
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// A thunk for \RMD callback methods to handle recv messages.
template <class T, void (T::*MessageHandlerMethod)(const RecvMessageDescriptor&)>
void MessageHandler(const RecvMessageDescriptor& desc, void* arg)
//-----------------------------------------------------------------------------
{
	(reinterpret_cast<T*>(arg)->*MessageHandlerMethod)(desc);
}

//-----------------------------------------------------------------------------
/// A thunk for pointer/length callback methods to handle recv messages.
template <class T, void (T::*MessageHandlerMethod)(uint32_t, const void*, uint32_t)>
void MessageHandlerP(const RecvMessageDescriptor& desc, void* arg)
//-----------------------------------------------------------------------------
{
	uint32_t msgID = desc.MessageID();
	if (desc.Contiguous()) {
		(reinterpret_cast<T*>(arg)->*MessageHandlerMethod)
			(msgID, desc.Buf(), desc.Size());
	} else {
		std::pair<char*,ptrdiff_t> tmp = GetTempBufferFromRMD(desc);
		(reinterpret_cast<T*>(arg)->*MessageHandlerMethod)
			(msgID, tmp.first, tmp.second);
		std::return_temporary_buffer(tmp.first);
	}
}

//-----------------------------------------------------------------------------
/// A thunk for callback methods to handle dropped segment reports.
template <class T, void (T::*DropReportMethod)(uint32_t, uint32_t)>
void DropReportHandler(uint32_t numSegments, uint32_t numBytes, void* arg)
//-----------------------------------------------------------------------------
{
	(reinterpret_cast<T*>(arg)->*DropReportMethod)(numSegments, numBytes);
}

//-----------------------------------------------------------------------------
/// A thunk for callback methods to handle connection events.
template <class T, void (T::*ConnectionEventMethod)(int)>
void ConnectionEventHandler(int which, void* arg)
//-----------------------------------------------------------------------------
{
	(reinterpret_cast<T*>(arg)->*ConnectionEventMethod)(which);
}

//-----------------------------------------------------------------------------
/// A thunk for callback methods to handle registration events.
template <class T, void (T::*RegistrationHandlerMethod)(bool reg,
	int16_t clientID, int16_t groupID, const uint32_t msgIDs[], unsigned count)>
void RegistrationHandler(bool reg, int16_t clientID, int16_t groupID,
	const uint32_t msgIDs[], unsigned count, void* arg)
//-----------------------------------------------------------------------------
{
	(reinterpret_cast<T*>(arg)->*RegistrationHandlerMethod)(reg, clientID,
		groupID, msgIDs, count);
}

} // namespace Thunk
} // namespace MCSB

#endif
