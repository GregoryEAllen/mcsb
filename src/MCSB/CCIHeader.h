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

#ifndef MCSB_CCIHeader_h
#define MCSB_CCIHeader_h
#pragma once

namespace MCSB {

enum {
	kCCIMessageID = 0xFFFFFFFF,
	kCCIBcastID = 0xFFFFFFFF
};

struct CCIHeader {
	uint32_t cciMsgID;
	uint16_t srcClientID;
	uint16_t dstClientID;
	CCIHeader(uint32_t mid, uint16_t src, uint16_t dst=0):
		cciMsgID(mid), srcClientID(src), dstClientID(dst) {}
};

} // namespace MCSB

#endif
