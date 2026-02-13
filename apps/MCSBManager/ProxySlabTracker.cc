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

#include "MCSB/ProxySlabTracker.h"
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
Manager::ClientProxy::SlabTracker::SlabTracker(void)
//-----------------------------------------------------------------------------
:	numProdSlabs(0), numConsSlabs(0), prodSlabsHeld(0), consSlabsHeld(0)
{
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SlabTracker::TotalNumSlabs(uint32_t n)
//-----------------------------------------------------------------------------
{
	prodSlabRefcnt.resize(n,0);
	consSlabRefcnt.resize(n,0);
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SlabTracker::InsertProdSlabs(
	const uint32_t slabIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	int errs = 0;
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		if (prodSlabRefcnt[slabID]) {
			errs++;
		} else {
			prodSlabsHeld++;
		}
		prodSlabRefcnt[slabID] = 1;
	}
	if (errs) {
		throw std::runtime_error("SlabTracker::InsertProdSlabs already holds slab");
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SlabTracker::EraseProdSlabs(
	const uint32_t slabIDs[], unsigned count)
// if there are errors, throw and don't erase anything
//-----------------------------------------------------------------------------
{
	if (count>prodSlabsHeld) {
		throw std::runtime_error("SlabTracker::EraseProdSlabs count>prodSlabsHeld");
	}
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		if (!prodSlabRefcnt[slabID]) {
			throw std::runtime_error("SlabTracker::EraseProdSlabs doesn't hold slab");
			// dtor will erase slabs we DO hold
		}
	}
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		prodSlabRefcnt[slabID] = 0;
		prodSlabsHeld--;
	}
}

//-----------------------------------------------------------------------------
unsigned Manager::ClientProxy::SlabTracker::GetAndEraseProdSlabs(
	uint32_t slabIDs[], unsigned mxcount)
// returns number removed
//-----------------------------------------------------------------------------
{
	unsigned count = 0;
	for (unsigned sid=0; sid<prodSlabRefcnt.size() && count<mxcount; sid++) {
		if (prodSlabRefcnt[sid]) {
			prodSlabRefcnt[sid] = 0;
			slabIDs[count++] = sid;
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
unsigned Manager::ClientProxy::SlabTracker::GetAndEraseConsSlabs(
	uint32_t slabIDs[], unsigned refcnts[], unsigned mxcount)
// returns number removed
//-----------------------------------------------------------------------------
{
	unsigned count = 0;
	for (unsigned sid=0; sid<consSlabRefcnt.size() && count<mxcount; sid++) {
		if (consSlabRefcnt[sid]) {
			slabIDs[count] = sid;
			refcnts[count++] = consSlabRefcnt[sid];
			consSlabRefcnt[sid] = 0;
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
bool Manager::ClientProxy::SlabTracker::TakeConsSlab(uint32_t slabID)
// return true on success
//-----------------------------------------------------------------------------
{
	bool success = 1;
	if (consSlabsHeld<numConsSlabs) {
		if (!consSlabRefcnt[slabID]++)
			consSlabsHeld++;
	} else {
		if (consSlabRefcnt[slabID])
			consSlabRefcnt[slabID]++;
		else
			success = 0;
	}
	return success;
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SlabTracker::DecrementConsSlabs(
	const uint32_t slabIDs[], unsigned count)
// if there are any errors, throw and don't decrement anything
//-----------------------------------------------------------------------------
{
	unsigned errs = 0;
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		if (!consSlabRefcnt[slabID]--) { // not held
			errs++;
		}
		// refcnt hit zero
		if (!consSlabRefcnt[slabID] && !consSlabsHeld--) { // invalid consSlabsHeld
			errs++;
		}
	}
	if (!errs) return;
	
	// undo because we are going to throw
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		if (!consSlabRefcnt[slabID]++) {
			consSlabsHeld++;
		}
	}
	throw std::runtime_error("SlabTracker::ReleaseConsSlabs encountered errors");
}

} // namespace MCSB
