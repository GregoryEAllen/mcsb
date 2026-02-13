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

#include "MCSB/uptimer.h"

#include <sys/time.h>
#include <stdio.h>
#include <math.h>

namespace MCSB {

//-----------------------------------------------------------------------------
double uptimer::CurrentTime(void)
//-----------------------------------------------------------------------------
{
	struct timeval tv;
	gettimeofday(&tv,0);
	return tv.tv_sec + tv.tv_usec/1e6;
}

//-----------------------------------------------------------------------------
char* uptimer::uptime(char* str, int len)
//-----------------------------------------------------------------------------
{
	double uptm = uptime();
	unsigned h = (unsigned)floor(uptm/3600);
	uptm -= h*3600;
	unsigned m = (unsigned)floor(uptm/60);
	uptm -= m*60;
	unsigned s = (unsigned)floor(uptm+0.5); // round seconds

	snprintf(str,len,"%02d:%02d:%02d", h,m,s);
	return str;
}

} // namespace MCSB
