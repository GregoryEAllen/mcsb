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

#include "rand_buf.h"

namespace MCSB {

//-----------------------------------------------------------------------------
void set_rand_buf(uint32_t* ptr, unsigned len, uint32_t& seed, uint32_t feed)
// with a simple lfsr
//-----------------------------------------------------------------------------
{
	for (unsigned n=0; n<len; n++) {
		seed = (seed & 1) ? (seed >> 1) ^ feed : (seed >> 1);
		ptr[n] = seed;
	}
}

//-----------------------------------------------------------------------------
unsigned verify_rand_buf(const uint32_t* ptr, unsigned len, uint32_t& seed, uint32_t feed)
// with a simple lfsr, return the number of errors
//-----------------------------------------------------------------------------
{
	unsigned errs = 0;
	for (unsigned n=0; n<len; n++) {
		seed = (seed & 1) ? (seed >> 1) ^ feed : (seed >> 1);
		if (ptr[n] != seed) errs++;
	}
	return errs;
}

//-----------------------------------------------------------------------------
unsigned set_and_verify_rand_buf(void* sptr, const void* vptr, unsigned len,
	uint32_t& seed, uint32_t feed)
//-----------------------------------------------------------------------------
{
	uint32_t vseed = seed;
	set_rand_buf((uint32_t*)sptr, len/sizeof(uint32_t), seed, feed);
	return verify_rand_buf((const uint32_t*)vptr, len/sizeof(uint32_t), vseed, feed);
}

} // namespace MCSB
