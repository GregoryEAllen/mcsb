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

#include "MCSB/DropReporter.h"

#include <limits>

namespace MCSB {

//-----------------------------------------------------------------------------
void DropReporter::Dropped(uint32_t segs, uint32_t bytes)
//-----------------------------------------------------------------------------
{
	if (!segs&&!bytes) return;
	needReport |= 1;
	segsDropped += segs;
	bytesDropped += bytes;
}

//-----------------------------------------------------------------------------
bool DropReporter::GetReport(uint32_t& segs, uint32_t& bytes)
//-----------------------------------------------------------------------------
{
	if (!needReport || throttle) return 0;
	uint64_t max = std::numeric_limits<uint32_t>::max();
	if (segsDropped>max) segsDropped = max;
	if (bytesDropped>max) bytesDropped = max;
	segs = segsDropped;
	bytes = bytesDropped;
	Reset();
	throttle = 1;
	return 1;
}

//-----------------------------------------------------------------------------
void DropReporter::Reset(void)
//-----------------------------------------------------------------------------
{
	needReport = 0;
	throttle = 0;
	segsDropped = 0;
	bytesDropped = 0;
}

} // namespace MCSB
