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

#ifndef MCSB_rand_buf_h
#define MCSB_rand_buf_h
#pragma once

#include <stdint.h>

namespace MCSB {

enum { kDefaultRandPoly = 0xfab57454 };

void set_rand_buf(uint32_t* ptr, uint32_t len, uint32_t& seed,
	uint32_t feed=kDefaultRandPoly);

uint32_t verify_rand_buf(const uint32_t* ptr, uint32_t len, uint32_t& seed,
	uint32_t feed=kDefaultRandPoly); // returns num errors

uint32_t set_and_verify_rand_buf(void* sptr, const void* vptr, uint32_t len,
	uint32_t& seed, uint32_t feed=kDefaultRandPoly);

} // namespace MCSB

#endif
