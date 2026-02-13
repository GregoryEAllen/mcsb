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

/// \file dbprinter.h
/// \brief A simple debug printer class used in MCSB.
///
/// dbprinter defines debug print levels that are similar to RFC5424 for syslog.
/// The MCSB verbosity set in the Manager and in a Client use these levels.
/// kNotice is generally the default level.

#ifndef MCSB_dbprinter_h
#define MCSB_dbprinter_h
#pragma once

#include <stdarg.h>

namespace MCSB {

/// A simple debug printer class used in MCSB.

/// dbprinter is used throughout MCSB, both internally and in the public
/// interface. It provides the ability to set debug print verbosity at runtime.
class dbprinter {
  public:
	/// Construct, setting the verbosity level
	dbprinter(int verbosity_=kNotice): verbosity(verbosity_) {}

	/// behaves like fprintf(stderr,...) if verbosity is at or above the specified level
	int dbprintf(int level, const char *fmt, ...) const;
	/// behaves like vfprintf(stderr,...) if verbosity is at or above the specified level
	int dbprintf(int level, const char *fmt, va_list ap) const;

	/// debug print levels are similar to RFC5424 for syslog
	enum { kCritical=0, kError, kWarning, kNotice, kInfo, kDebug };

	/// Get the current verbosity level.
	int Verbosity(void) const { return verbosity; }
	/// Set the current verbosity level.
	int Verbosity(int v) { return verbosity = v; }
  private:
	int verbosity;
};

/// debug print levels are similar to RFC5424 for syslog
enum { kCritical=0, kError, kWarning, kNotice, kInfo, kDebug };

} // namespace MCSB

#endif
