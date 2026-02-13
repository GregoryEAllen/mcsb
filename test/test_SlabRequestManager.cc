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

// always assert for tests, even in Release
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "MCSB/SlabRequestManager.h"
#include <cstdio>
#include <cassert>

//-----------------------------------------------------------------------------
int test1(void)
//-----------------------------------------------------------------------------
{
	MCSB::SlabRequestManager srm;
	
	size_t emptyRequestsSize = srm.EmptyRequestsSize();
	
	for (int lvl=srm.MaxLevel(); lvl>=0; lvl--) {
		srm.AddRequest(lvl,lvl,2*lvl+1);
		printf("srm.LowestLevelPending(): %d\n", srm.LowestLevelPending());
		assert(srm.LowestLevelPending()==lvl);
		srm.AddRequest(lvl,lvl,2*lvl+2);
		assert(srm.NumRequests(lvl)==2);
	}
	assert(srm.NumRequests()==srm.MaxLevel()*2+2);
	
	unsigned lastID = 0;
	while (srm.NumRequests()) {
		unsigned lvl = srm.LowestLevelPending();
		const MCSB::SlabRequestManager::SlabRequest& req = srm.FrontRequest(lvl);
		printf("req.Id(): %d, lastID %d\n", req.Id(), lastID);
		assert(req.Id()>lastID);
		assert(req.NumSlabs()==lvl);
		lastID = req.Id();
		srm.PopFrontRequest(lvl);
	}
	
	for (int i=0; i<128; i++) {
		srm.AddRequest(0,1,i);
	}
	srm.Reset();
	assert(srm.NumRequests()==0);
	printf("emptyRequestsSize: %lu\n", emptyRequestsSize);
	printf("srm.EmptyRequestsSize(): %lu\n", srm.EmptyRequestsSize());
	assert(emptyRequestsSize==srm.EmptyRequestsSize());
	
	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	int result = test1();
	if (result) return result;
	fprintf(stderr,"=== PASS ===\n");
	return result;
}
