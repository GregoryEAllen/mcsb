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

/// \file VariantClientOptions.h
/// \brief Help creating ClientOptions using \libvariant's ArgParse.

#ifndef MCSB_VariantClientOptions_h
#define MCSB_VariantClientOptions_h
#pragma once

#include "MCSB/ClientOptions.h"
#include "Variant/ArgParse.h"

namespace MCSB {

/// Put libvariant's ArgParse into MCSB namespace for convenience.
typedef libvariant::ArgParse ArgParse;
/// Put libvariant's Variant into MCSB namespace for convenience.
typedef libvariant::Variant  Variant;

/// Create a ClientOptions object from argc/argv using \libvariant's ArgParse with defaults.
ClientOptions VariantClientOptions(int argc, char* const argv[]);
ClientOptions VariantClientOptions(const Variant& v);

/// An options setter class for creating ClientOptions using \libvariant's ArgParse.
class VariantClientOptionsHelper {
  public:
	/// Install MCSB options into an ArgParse object.
	
	/// \param args_       The ArgParse object to be used for parsing.
	/// \param shortopts   0: do not install short options,
	/// 1: install non-advanced short options (default),
	/// 2: install all short options.
	/// \see examples/ex4_VariantClientOptions.cc
	VariantClientOptionsHelper(ArgParse& args_, int shortopts=1): args(args_)
		{ AddDefaultOptions(args_.GetProgramName().c_str(), shortopts); }

	/// Set the default number of producer bytes (bytes=0 also sets slabs to 0).
	VariantClientOptionsHelper& DefaultMinProducerBytes(size_t bytes);
	/// Set the default number of consumer bytes (bytes=0 also sets slabs to 0).
	VariantClientOptionsHelper& DefaultMinConsumerBytes(size_t bytes);
	/// Set the default minimum number of producer slabs.
	VariantClientOptionsHelper& DefaultMinProducerSlabs(uint32_t slabs);
	/// Set the default minimum number of consumer slabs.
	VariantClientOptionsHelper& DefaultMinConsumerSlabs(uint32_t slabs);
	/// Set the default MCSB verbosity using values in dbprinter.h.
	VariantClientOptionsHelper& DefaultVerbosity(int verbosity);

	/// Create a ClientOptions object from the parsed arguments in Variant v.
	static ClientOptions VariantClientOptions(const Variant& v);

  protected:
	/// Alternate constructor to support deprecated legacy ClientOptionsFromVariant.
	// (does not call AddDefaultOptions())
	// dummy just to disambiguate based on type
	VariantClientOptionsHelper(ArgParse& args_, const char* dummy)
		: args(args_) {}

  private:
	ArgParse& args;
	void AddDefaultOptions(const char* argv0, int shortopts);
};

/// Create a ClientOptions object from the parsed arguments in Variant v.
inline ClientOptions VariantClientOptions(const Variant& v)
{ return VariantClientOptionsHelper::VariantClientOptions(v); }

} // namespace MCSB

#endif
