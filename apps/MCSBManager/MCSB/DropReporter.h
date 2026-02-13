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

#ifndef MCSB_DropReporter_h
#define MCSB_DropReporter_h
#pragma once

#include <stdint.h>

namespace MCSB {

class DropReporter {
  public:
	DropReporter(void) { Reset(); }
	
	void Dropped(uint32_t segs, uint32_t bytes);
	void Ack(void) { throttle = 0; }
	bool GetReport(uint32_t& segs, uint32_t& bytes);

  protected:
	void Reset(void);
	bool needReport;
	bool throttle;
	uint64_t segsDropped;
	uint64_t bytesDropped;
};

} // namespace MCSB

#endif
