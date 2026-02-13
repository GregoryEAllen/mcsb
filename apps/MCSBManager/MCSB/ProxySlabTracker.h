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

#ifndef MCSB_ProxySlabTracker_h
#define MCSB_ProxySlabTracker_h
#pragma once

#include "MCSB/ClientProxy.h"
#include <vector>

namespace MCSB {

class Manager::ClientProxy::SlabTracker {
  public:
	SlabTracker(void);

	void TotalNumSlabs(uint32_t n);

	uint32_t NumProdSlabs(void) const { return numProdSlabs; }
	uint32_t NumProdSlabs(uint32_t n) { return numProdSlabs=n; }
	uint32_t NumConsSlabs(void) const { return numConsSlabs; }
	uint32_t NumConsSlabs(uint32_t n) { return numConsSlabs=n; }

	unsigned ProdSlabsHeld(void) const
		{ return prodSlabsHeld; }
	unsigned ProdSlabsNeeded(void) const
		{ return numProdSlabs>prodSlabsHeld ? numProdSlabs-prodSlabsHeld : 0; }
	unsigned ConsSlabsHeld(void) const
		{ return consSlabsHeld; }

	void InsertProdSlabs(const uint32_t slabIDs[], unsigned count);
	void EraseProdSlabs(const uint32_t slabIDs[], unsigned count);
	
	bool TakeConsSlab(uint32_t slabID);
	void DecrementConsSlabs(const uint32_t slabIDs[], unsigned count);

	// these are slow and only occur at dtor time
	unsigned GetAndEraseProdSlabs(uint32_t slabIDs[], unsigned mxcount);
	unsigned GetAndEraseConsSlabs(uint32_t slabIDs[], unsigned refcnts[], unsigned mxcount);

  protected:
	uint32_t numProdSlabs, numConsSlabs;
	unsigned prodSlabsHeld, consSlabsHeld;
	std::vector<bool> prodSlabRefcnt;
	std::vector<uint16_t> consSlabRefcnt;
};

} // namespace MCSB

#endif
