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

#ifndef MCSB_StdClientOptions_h
#define MCSB_StdClientOptions_h
#pragma once

namespace MCSB {

static const char* const kPath_minProducerBytes = "mcsb/minProducerBytes";
static const char* const kPath_minConsumerBytes = "mcsb/minConsumerBytes";
static const char* const kPath_minProducerSlabs = "mcsb/minProducerSlabs";
static const char* const kPath_minConsumerSlabs = "mcsb/minConsumerSlabs";
static const char* const kPath_ctrlSockName     = "mcsb/ctrlSockName";
static const char* const kPath_clientName       = "mcsb/clientName";
static const char* const kPath_crcPolicy        = "mcsb/crcPolicy";
static const char* const kPath_verbosity        = "mcsb/verbosity";

static const char* const kLOpt_minProducerBytes = "mcsb-prod-bytes";
static const char* const kLOpt_minConsumerBytes = "mcsb-cons-bytes";
static const char* const kLOpt_minProducerSlabs = "mcsb-prod-slabs";
static const char* const kLOpt_minConsumerSlabs = "mcsb-cons-slabs";
static const char* const kLOpt_ctrlSockName     = "mcsb-ctrl-sock";
static const char* const kLOpt_clientName       = "mcsb-client-name";
static const char* const kLOpt_crcPolicy        = "mcsb-crc-policy";
static const char* const kLOpt_verbosity        = "mcsb-verbosity";

static const char* const kEnv_minProducerBytes  = "MCSB_PROD_BYTES";
static const char* const kEnv_minConsumerBytes  = "MCSB_CONS_BYTES";
static const char* const kEnv_minProducerSlabs  = "MCSB_PROD_SLABS";
static const char* const kEnv_minConsumerSlabs  = "MCSB_CONS_SLABS";
static const char* const kEnv_ctrlSockName      = "MCSB_CTRL_SOCK";
static const char* const kEnv_clientName        = "MCSB_CLIENT_NAME";
static const char* const kEnv_crcPolicy         = "MCSB_CRC_POLICY";
static const char* const kEnv_verbosity         = "MCSB_VERBOSITY";

} // namespace MCSB

#endif
