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

//	Create ClientOptions using libvariant::ArgParse

#include "MCSB/VariantClientOptions.h"
#include "MCSB/StdClientOptions.h"

namespace MCSB {

//-----------------------------------------------------------------------------
void VariantClientOptionsHelper::AddDefaultOptions(const char* argv0, int shortopts)
//-----------------------------------------------------------------------------
{
	bool advshorts = shortopts>1;
	
	args.AddOption(kPath_minProducerBytes, shortopts?'b':0, kLOpt_minProducerBytes,
		"MCSB Client minimum producer bytes")
		.Type(libvariant::ARGTYPE_UINT)
		.Minimum(1)
		.Default(ClientOptions::kDefaultMinProducerBytes)
		.Env(kEnv_minProducerBytes);

	args.AddOption(kPath_minConsumerBytes, shortopts?'B':0, kLOpt_minConsumerBytes,
		"MCSB Client minimum consumer bytes")
		.Type(libvariant::ARGTYPE_UINT)
		.Minimum(1)
		.Default(ClientOptions::kDefaultMinConsumerBytes)
		.Env(kEnv_minConsumerBytes);

	args.AddOption(kPath_minProducerSlabs, advshorts?'s':0, kLOpt_minProducerSlabs,
		"MCSB Client minimum producer slabs")
		.Type(libvariant::ARGTYPE_UINT)
		.Minimum(1)
		.Default(ClientOptions::kDefaultMinProducerSlabs)
		.Env(kEnv_minProducerSlabs)
		.Advanced();

	args.AddOption(kPath_minConsumerSlabs, advshorts?'S':0, kLOpt_minConsumerSlabs,
		"MCSB Client minimum consumer slabs")
		.Type(libvariant::ARGTYPE_UINT)
		.Minimum(1)
		.Default(ClientOptions::kDefaultMinConsumerSlabs)
		.Env(kEnv_minConsumerSlabs)
		.Advanced();

	std::string s;
	args.AddOption(kPath_ctrlSockName, shortopts?'c':0, kLOpt_ctrlSockName,
		"MCSB control socket name")
		.Type(libvariant::ARGTYPE_STRING)
		.Default(ClientOptions::SetDefaultCtrlSocketName(s))
		.Env(kEnv_ctrlSockName);

	args.AddOption(kPath_clientName, advshorts?'n':0, kLOpt_clientName,
		"MCSB Client name")
		.Type(libvariant::ARGTYPE_STRING)
		.Default(ClientOptions::SetDefaultClientName(s, argv0))
		.Env(kEnv_clientName)
		.Advanced();

	args.AddOption(kPath_crcPolicy, advshorts?'p':0, kLOpt_crcPolicy,
		"MCSB Client CRC policy")
		.AddChoice(ClientOptions::CrcPolicyStr(ClientOptions::eNoCrcs))
		.AddChoice(ClientOptions::CrcPolicyStr(ClientOptions::eSetCrcs))
		.AddChoice(ClientOptions::CrcPolicyStr(ClientOptions::eVerifyCrcs))
		.AddChoice(ClientOptions::CrcPolicyStr(ClientOptions::eSetAndVerifyCrcs))
		.AddChoice(ClientOptions::CrcPolicyStr(ClientOptions::eDefaultCrcStr))
		.Default(ClientOptions::DefaultCrcStr())
		.Env(kEnv_crcPolicy)
		.Advanced();

	args.AddOption(kPath_verbosity, 0, kLOpt_verbosity,
		"MCSB Client verbosity")
		.Type(libvariant::ARGTYPE_UINT)
		.Maximum(kDebug)
		.Default(ClientOptions::kDefaultVerbosity)
		.Env(kEnv_verbosity)
		.Advanced();

	args.AddGroup("MCSB")
		.Add(kPath_minProducerBytes)
		.Add(kPath_minConsumerBytes)
		.Add(kPath_minProducerSlabs)
		.Add(kPath_minConsumerSlabs)
		.Add(kPath_ctrlSockName)
		.Add(kPath_clientName)
		.Add(kPath_crcPolicy)
		.Add(kPath_verbosity)
		.Title("MCSB Client Options:");
}

//-----------------------------------------------------------------------------
ClientOptions VariantClientOptionsHelper::VariantClientOptions(const Variant& v)
//-----------------------------------------------------------------------------
{
	ClientOptions opts;
	v.GetPathInto(opts.minProducerBytes, kPath_minProducerBytes);
	v.GetPathInto(opts.minConsumerBytes, kPath_minConsumerBytes);
	v.GetPathInto(opts.minProducerSlabs, kPath_minProducerSlabs);
	v.GetPathInto(opts.minConsumerSlabs, kPath_minConsumerSlabs);
	v.GetPathInto(opts.ctrlSockName, kPath_ctrlSockName);
	v.GetPathInto(opts.clientName, kPath_clientName);
	const char* policyStr = v.GetPath(kPath_crcPolicy).AsString().c_str();
	opts.SetCrcPolicy(policyStr);
	v.GetPathInto(opts.verbosity, kPath_verbosity);
	return opts;
}

//-----------------------------------------------------------------------------
VariantClientOptionsHelper&
	VariantClientOptionsHelper::DefaultMinProducerBytes(size_t bytes)
//-----------------------------------------------------------------------------
{
	args.At(kPath_minProducerBytes).Default(bytes);
	if (!bytes)
		DefaultMinProducerSlabs(0);
	return *this;
}

//-----------------------------------------------------------------------------
VariantClientOptionsHelper&
	VariantClientOptionsHelper::DefaultMinConsumerBytes(size_t bytes)
//-----------------------------------------------------------------------------
{
	args.At(kPath_minConsumerBytes).Default(bytes);
	if (!bytes)
		DefaultMinConsumerSlabs(0);
	return *this;
}

//-----------------------------------------------------------------------------
VariantClientOptionsHelper&
	VariantClientOptionsHelper::DefaultMinProducerSlabs(uint32_t slabs)
//-----------------------------------------------------------------------------
{
	args.At(kPath_minProducerSlabs).Default(slabs);
	return *this;
}

//-----------------------------------------------------------------------------
VariantClientOptionsHelper&
	VariantClientOptionsHelper::DefaultMinConsumerSlabs(uint32_t slabs)
//-----------------------------------------------------------------------------
{
	args.At(kPath_minConsumerSlabs).Default(slabs);
	return *this;
}

//-----------------------------------------------------------------------------
VariantClientOptionsHelper&
	VariantClientOptionsHelper::DefaultVerbosity(int verbosity)
//-----------------------------------------------------------------------------
{
	args.At(kPath_verbosity).Default(verbosity);
	return *this;
}

//-----------------------------------------------------------------------------
ClientOptions VariantClientOptions(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	ArgParse argParse;
	VariantClientOptionsHelper vopts(argParse);
	Variant var = argParse.Parse(argc, argv);
	return VariantClientOptions(var);
}

} // namespace MCSB
