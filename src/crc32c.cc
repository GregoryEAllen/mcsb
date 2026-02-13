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

#include "MCSB/crc32c.h"

namespace MCSB {

// support for CRC-32C (Castagnoli)
static const uint32_t kPolynomialCRC32C = 0x82F63B78;
static uint32_t crc32c_table[256] = { 1 };
static bool isAccelerated = 0;

// note that table[0]=0 after initialization


//-----------------------------------------------------------------------------
void initialize_crc_table(uint32_t* table, uint32_t poly=kPolynomialCRC32C)
//	compute the table given the desired crc polynimial
//-----------------------------------------------------------------------------
{
	for (unsigned i=0; i<256; i++) {
		uint32_t crc = (uint32_t) i;
		for (unsigned b=0; b<8; b++) {
			if (crc & 1)
				crc = poly ^ (crc >> 1);
			else
				crc = crc >> 1;
		}
		table[i] = crc;
	}
}


#if defined(__i386__) || defined(__x86_64__)
//-----------------------------------------------------------------------------
inline static uint32_t __attribute__((__always_inline__))
	crc32c_sse42_uint8(uint32_t crc, uint8_t data)
//	adds data to the running crc, returns result
//	refer to Intel SSE4.2 Programming Reference
//-----------------------------------------------------------------------------
{
	__asm__ __volatile__(
		".byte 0xf2, 0x0f, 0x38, 0xf0, 0xf1;"
		: "=S" (crc)
		: "0" (crc), "c" (data)
	);
	return crc;
}

//-----------------------------------------------------------------------------
inline static uint32_t __attribute__((__always_inline__))
	crc32c_sse42_uint32(uint32_t crc, uint32_t data)
//	adds data to the running crc, returns result
//	refer to Intel SSE4.2 Programming Reference
//-----------------------------------------------------------------------------
{
	__asm__ __volatile__(
		".byte 0xf2, 0x0f, 0x38, 0xf1, 0xf1;"
		: "=S" (crc)
		: "0" (crc), "c" (data)
	);
	return crc;
}

//-----------------------------------------------------------------------------
void x86_cpuid(uint32_t regs[4])
//	access the info provided by the CPUID instruction on Intel x86
//	input: regs[0] contains function
//	output: regs contains eax, ebx, ecx, edx
//-----------------------------------------------------------------------------
{
#if defined(__i386__) && defined(__PIC__)
	__asm__ ( \
		"xchgl %%ebx, %1\n" \
		"cpuid\n" \
		"xchgl %%ebx, %1\n" \
		: "=a" (regs[0]), "=r" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) \
		: "0" (regs[0]) \
	);
#else
	__asm__ ( \
		"cpuid\n" \
		: "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) \
		: "0" (regs[0]) \
	);
#endif
}
#endif


//-----------------------------------------------------------------------------
uint32_t update_crc32c_sse42(uint32_t crc, const void *buf, size_t len)
//-----------------------------------------------------------------------------
{
#if defined(__i386__) || defined(__x86_64__)
	const uint8_t* b = (const uint8_t*)buf;

	while (long(b)&3 && len--) {
		crc = crc32c_sse42_uint8(crc,*b++);
	}
	
	while (len>=4) {
		crc = crc32c_sse42_uint32(crc,*((uint32_t*)b));
		len -= 4;
		b += 4;
	}
	
	while (len--) {
		crc = crc32c_sse42_uint8(crc,*b++);
	}
#endif

	return crc;
}

//-----------------------------------------------------------------------------
uint32_t update_crc32c(uint32_t crc, const void *buf, size_t len)
//-----------------------------------------------------------------------------
{
	// short cut for when we already know it's accelerated
	if (isAccelerated)
		return update_crc32c_sse42(crc,buf,len);

	// make sure the table is initialized
	if (crc32c_table[0]) {
		if (crc32c_is_accelerated())
			return update_crc32c_sse42(crc,buf,len);
	}
	
	const uint8_t* b = (const uint8_t*)buf;
	for (size_t i=0; i<len; i++) {
		crc = crc32c_table[(crc ^ b[i]) & 0xff] ^ (crc>>8);
	}
	return crc;
}

//-----------------------------------------------------------------------------
uint32_t crc32c(const void *buf, size_t len)
//-----------------------------------------------------------------------------
{
	return update_crc32c(0xffffffff, buf, len) ^ 0xffffffff;
}

//-----------------------------------------------------------------------------
bool crc32c_is_accelerated(bool useAccelerated)
//-----------------------------------------------------------------------------
{
	if (crc32c_table[0]) {
		initialize_crc_table(crc32c_table);
	}
	
	int have_sse42 = 0;
#if defined(__i386__) || defined(__x86_64__)
	if (useAccelerated) {
		uint32_t regs[4];
		regs[0] = 1; // processor info and feature bits
		x86_cpuid(regs);
		have_sse42 = !!( regs[2] & (1<<20) ); // this bit of ECX is SSE4.2
	}
#endif

	isAccelerated = useAccelerated && have_sse42;
	return isAccelerated;
}

};
