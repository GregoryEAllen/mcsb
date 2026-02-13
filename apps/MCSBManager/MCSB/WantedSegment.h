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

#ifndef MCSB_WantedSegment_h
#define MCSB_WantedSegment_h
#pragma once

#include "MCSB/IntrusiveList.h"
#include <stdint.h>

namespace MCSB {

class WantedSegment :
	public IntrusiveList<WantedSegment>::Hook,
	public IntrusiveList<WantedSegment,int>::Hook
{
  public:
	WantedSegment(void): clientID(-1), blockID(-1) {}

	uint16_t ClientID(void) const { return clientID; }
	uint32_t BlockID(void) const { return blockID; }

  protected:
	uint16_t clientID; // of the client wanting this segment
	uint32_t blockID;  // of the segment
	friend class SlabManager;
};

typedef IntrusiveList<WantedSegment> WantedSegmentCProxyList;
typedef IntrusiveList<WantedSegment,int> WantedSegmentMgrList;

} // namespace MCSB

#endif
