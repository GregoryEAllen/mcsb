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

#include "MCSB/GroupManager.h"
#include <stdexcept>
#include <cstdio>

namespace MCSB {

//-----------------------------------------------------------------------------
GroupManager::GroupManager(void)
//-----------------------------------------------------------------------------
:	nextNewGroupID(1)
{
}

//-----------------------------------------------------------------------------
GroupManager::~GroupManager(void)
//-----------------------------------------------------------------------------
{
	if (strToGroupID.size() || groupIDToStr.size()) {
		fprintf(stderr, "### GroupManager::~GroupManager maps not empty\n");
	}
}

//-----------------------------------------------------------------------------
short GroupManager::GetGroupID(const char* groupStr, bool create, bool inc)
//-----------------------------------------------------------------------------
{
	std::string newGroupStr = groupStr;
	if (!newGroupStr.size()) return 0; // empty string is group 0

	StrToGroupIDMap::iterator it = strToGroupID.find(newGroupStr);
	if (it!=strToGroupID.end()) {
		if (inc) IncGroupID(it->second);
		return it->second;
	}

	short gid = -1;
	if (create) gid = NewGroupStr(groupStr);
	if (inc) IncGroupID(gid);
	return gid;
}

//-----------------------------------------------------------------------------
short GroupManager::NewGroupStr(std::string groupStr)
//-----------------------------------------------------------------------------
{
	while (strToGroupID.size()<kMaxGroupID) {
		short newGroupID = nextNewGroupID++;
		if (nextNewGroupID>kMaxGroupID) {
			nextNewGroupID = 1;
		}
		GroupIDToStrMap::iterator it = groupIDToStr.find(newGroupID);
		if (it!=groupIDToStr.end()) continue; // if found
		
		// insert into both maps
		strToGroupID.insert(std::make_pair(groupStr,newGroupID));
		groupIDToStr.insert(std::make_pair(newGroupID,std::make_pair(groupStr,0)));
		return newGroupID;
	}
	return -1;
}

//-----------------------------------------------------------------------------
const char* GroupManager::GetGroupStr(short gid) const
//-----------------------------------------------------------------------------
{
	GroupIDToStrMap::const_iterator it = groupIDToStr.find(gid);
	if (it==groupIDToStr.end()) return 0; // not found
	return it->second.first.c_str();
}

//-----------------------------------------------------------------------------
int GroupManager::IncGroupID(short gid)
//-----------------------------------------------------------------------------
{
	GroupIDToStrMap::iterator it = groupIDToStr.find(gid);
	if (it==groupIDToStr.end()) return -1; // not found
	return ++(it->second.second);
}

//-----------------------------------------------------------------------------
int GroupManager::DecGroupID(short gid)
//-----------------------------------------------------------------------------
{
	GroupIDToStrMap::iterator it = groupIDToStr.find(gid);
	if (it==groupIDToStr.end()) return -1; // not found
	int newCount = --(it->second.second);
	if (!newCount) {
		// erase from both maps
		strToGroupID.erase(it->second.first);
		groupIDToStr.erase(it);
	} else if (newCount<0) {
		throw std::runtime_error("GroupManager count underflow");
	}
	return newCount;
}

//-----------------------------------------------------------------------------
int GroupManager::GetGroups(short groups[], unsigned count) const
//-----------------------------------------------------------------------------
{
	if (count<NumGroups()) {
		return -NumGroups();
	}
	GroupIDToStrMap::const_iterator it = groupIDToStr.begin();
	for (; it!=groupIDToStr.end(); ++it) {
		*groups++ = it->first;
	}
	return NumGroups();
}

} // namespace MCSB
