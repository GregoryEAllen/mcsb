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

#include "MCSB/dbprinter.h"

#include <stdio.h>

namespace MCSB {

//-----------------------------------------------------------------------------
int dbprinter::dbprintf(int level, const char *fmt, ...) const
//-----------------------------------------------------------------------------
{
	va_list ap;
	if (level>verbosity) return 0;
	va_start(ap, fmt);
	int ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return ret;
}

//-----------------------------------------------------------------------------
int dbprinter::dbprintf(int level, const char *fmt, va_list ap) const
//-----------------------------------------------------------------------------
{
	return (level<=verbosity) ? vfprintf(stderr, fmt, ap) : 0;
}

} // namespace MCSB
