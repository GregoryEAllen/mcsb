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

//-----------------------------------------------------------------------------
//	32-bit Castagnoli cyclic redundancy check (CRC-32C)
//	with hardware acceleration if SSE4.2 is available
//-----------------------------------------------------------------------------

#ifndef crc32c_h
#define crc32c_h
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace MCSB {

// Return the crc32c of the buffer
uint32_t crc32c(const void *buf, size_t len);

//	Update a running crc32c
uint32_t update_crc32c(uint32_t crc, const void *buf, size_t len);
//	See source for initialization/completion steps

// true if crc32c is hardware accelerated
bool crc32c_is_accelerated(bool useAccelerated=true);

} // namespace MCSB

#endif
