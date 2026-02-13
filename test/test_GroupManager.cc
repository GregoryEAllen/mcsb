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

#include "MCSB/GroupManager.h"
#include <cstdio>
#include <cassert>
#include <cstring>

//-----------------------------------------------------------------------------
int test1(void)
//-----------------------------------------------------------------------------
{
	MCSB::GroupManager gm;
	
	// allocate 99 groups, testing GetGroupID and GetGroupStr
	for (int gid=1; gid<100; gid++) {
		char str[80];
		sprintf(str, "%d", gid);
		short newGid = gm.GetGroupID(str);
		assert(newGid==gid);
		const char* gstr = gm.GetGroupStr(gid);
		assert(!strcmp(gstr,str));
		short newGid2 = gm.GetGroupID(str);
		assert(newGid2==gid);
	}
	assert(99==gm.NumGroups());
	
	// test GetGroups
	short groupIDs[99];
	gm.GetGroups(groupIDs,99);
	
	// test inc and dec
	for (int gid=1; gid<100; gid++) {
		assert(groupIDs[gid-1]==gid);
		assert(3==gm.IncGroupID(gid));
		assert(2==gm.DecGroupID(gid));
		assert(1==gm.DecGroupID(gid));
		assert(0==gm.DecGroupID(gid));
	}
	
	// test deletion
	assert(0==gm.NumGroups());
	
	// test recycling
	while (1) {
		short newGid = gm.GetGroupID("test");
		assert(newGid==gm.GetGroupID("test"));
		assert(1==gm.DecGroupID(newGid));
		assert(0==gm.DecGroupID(newGid));
		if (newGid==1) break;
	}
	
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
