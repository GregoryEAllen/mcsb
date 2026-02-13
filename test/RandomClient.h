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

#ifndef MCSB_RandomClient_h
#define MCSB_RandomClient_h
#pragma once

#include "MCSB/Client.h"
#include "MCSB/dbprinter.h"

#include <vector>

namespace MCSB {

class RandomClient : public dbprinter {
  public:
	RandomClient(Client* client, int numMessages, int id, bool fillData=1, bool contiguous=1);
	virtual ~RandomClient(void);

	bool Done(void);
	
	unsigned TotalErrors(void) const { return totalErrs; }

  protected:
	void HandleMessage(uint32_t msgID, const void* buf, uint32_t len);
	void HandleDropReport(uint32_t segs, uint32_t bytes);
	void SendMessage(void);

	Client* client;
	int numMessages;
	uint32_t id;
	uint32_t msgID;
	uint32_t randSeed;
	uint32_t verfSeed;
	bool started;
	bool fillData;
	bool contiguous;
	uint32_t maxMsgSize;
	std::vector<uint32_t> messageBuffer;
	unsigned totalErrs;

	void SendMessage(uint32_t msgID, const void* buf, uint32_t len);
};

} // namespace MCSB

#endif
