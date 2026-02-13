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

#ifndef MCSB_GroupManager_h
#define MCSB_GroupManager_h
#pragma once

#include <map>
#include <string>

namespace MCSB {

class GroupManager {
  public:
	GroupManager(void);
	~GroupManager(void);
	enum { kMaxGroupID = 9999 };

	short GetGroupID(const char* groupStr, bool create=1, bool inc=1);
	const char* GetGroupStr(short gid) const;

	int IncGroupID(short gid);
	int DecGroupID(short gid);

	unsigned NumGroups(void) const { return strToGroupID.size(); }
	int GetGroups(short groups[], unsigned count) const;

  protected:
	typedef std::map<std::string,short> StrToGroupIDMap;
	StrToGroupIDMap strToGroupID;
	typedef std::map<short, std::pair<std::string,int> > GroupIDToStrMap;
	GroupIDToStrMap groupIDToStr;
	short nextNewGroupID;
	
	short NewGroupStr(std::string groupStr);
};

} // namespace MCSB

#endif
