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

#include "MCSB/IntrusiveList.h"

#include <vector>
#include <list>
#include <stdio.h>
#include <stdlib.h>


//-----------------------------------------------------------------------------
// Foo demonstrates multiple-inheritance with IntrusiveList,
// so a single Foo instantiation can be inserted in two different lists
struct FooTag;
class Foo : public MCSB::IntrusiveList<Foo>::Hook, public MCSB::IntrusiveList<Foo,FooTag>::Hook {
  public:
	Foo(void): foo_(0) {}
	Foo& operator=(int f) { foo_ = f; return *this; }
	operator int(void) { return foo_; }
	bool operator==(const Foo& rhs) const { return this == &rhs; }
  protected:
	int foo_;
};


//-----------------------------------------------------------------------------
template<typename ItT1, typename ItT2>
bool contents_equal(ItT1 it1, ItT1 end1, ItT2 it2, ItT2 end2)
//	contain the same values in the same order
//-----------------------------------------------------------------------------
{
	while (it1!=end1 && it2!=end2 && *it1==*it2) {
		printf("%d ", int(*it1));
		++it1;
		++it2;
	}
	printf("\n");
	return it1==end1 && it2==end2;
}


//-----------------------------------------------------------------------------
int test_IntrusiveList(unsigned N)
//-----------------------------------------------------------------------------
{

//	std::vector<Foo> fooNodes(N);	// can't work because copy ctor disallowed
	Foo* fooNodes = new Foo[N];

	// two independent IntrusiveLists
	MCSB::IntrusiveList<Foo> ilist1;
	MCSB::IntrusiveList<Foo,FooTag> ilist2;
	// use the behavior of std::list for comparison
	std::list<int> list1, list2;

	// check all of the iterator types
	MCSB::IntrusiveList<Foo>::iterator it = ilist1.begin();
	MCSB::IntrusiveList<Foo>::const_iterator cit = ilist1.begin();
	MCSB::IntrusiveList<Foo>::reverse_iterator rit = ilist1.rbegin();
	MCSB::IntrusiveList<Foo>::const_reverse_iterator crit = ilist1.rbegin();
	it = ilist1.end();
	cit = ilist1.end();
	rit = ilist1.rend();
	crit = ilist1.rend();

	// pre-fill the nodes with their index value
	for (unsigned i=0; i<N; i++) {
		fooNodes[i] = i;
	}

	// fill the lists
	for (unsigned i=0; i<N; i++) {
		ilist1.push_back( fooNodes[i] );
		ilist2.push_front( fooNodes[i] );
		list1.push_back( fooNodes[i] );
		list2.push_front( fooNodes[i]  );
	}
	
	// check that they're the same
	assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
	assert( contents_equal(ilist1.rbegin(),ilist1.rend(),list1.rbegin(),list1.rend()) );
	assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );
	assert( contents_equal(ilist2.rbegin(),ilist2.rend(),list2.rbegin(),list2.rend()) );

	// empty lists2
	while (list2.size()) {
		ilist2.pop_back();
		list2.pop_back();
	}

	// put lists1 in lists2
	ilist2.insert(ilist2.begin(), ilist1.begin(), ilist1.end());
	list2.insert(list2.begin(), list1.rbegin(), list1.rend());

	// check that they're the same
	assert( contents_equal(ilist2.begin(),ilist2.end(),list2.rbegin(),list2.rend()) );

	// empty all
	while (list1.size()) {
		ilist1.pop_back();
		ilist2.pop_back();
		list1.pop_back();
		list2.pop_back();
	}

	// insert the ith node into a random place
	for (unsigned i=0; i<N; i++) {
		Foo& foo = fooNodes[i];
		unsigned offset = i ? random() % i : 0;

		MCSB::IntrusiveList<Foo>::iterator iti1 = ilist1.begin();
		std::advance(iti1,offset);
		iti1 = ilist1.insert(iti1,foo);
		assert(foo==*iti1);

		std::list<int>::iterator it1 = list1.begin();
		std::advance(it1,offset);
		it1 = list1.insert(it1,foo);
		assert(foo==*it1);

		offset = i ? random() % i : 0;
		
		MCSB::IntrusiveList<Foo,FooTag>::iterator iti2 = ilist2.begin();
		std::advance(iti2,offset);
		iti2 = ilist2.insert(iti2,foo);
		assert(foo==*iti2);

		std::list<int>::iterator it2 = list2.begin();
		std::advance(it2,offset);
		it2 = list2.insert(it2,foo);
		assert(foo==*it2);
	}

	// check that they're the same
	assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
	assert( contents_equal(ilist1.rbegin(),ilist1.rend(),list1.rbegin(),list1.rend()) );
	assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );
	assert( contents_equal(ilist2.rbegin(),ilist2.rend(),list2.rbegin(),list2.rend()) );

	// erase from a random place
	for (unsigned i=0; i<N; i++) {
		unsigned offset = random() % ilist1.size();

		MCSB::IntrusiveList<Foo>::iterator iti1a, iti1 = ilist1.begin();
		std::advance(iti1,offset);
		iti1a = iti1;
		++iti1a;
		iti1 = ilist1.erase(iti1);
		assert(iti1a==iti1);

		std::list<int>::iterator it1a, it1 = list1.begin();
		std::advance(it1,offset);
		it1a = it1;
		++it1a;
		it1 = list1.erase(it1);
		assert(it1a==it1);

		assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );

		offset = random() % ilist2.size();
		
		MCSB::IntrusiveList<Foo,FooTag>::iterator iti2a, iti2 = ilist2.begin();
		std::advance(iti2,offset);
		iti2a = iti2;
		++iti2a;
		iti2 = ilist2.erase(iti2);
		assert(iti2a==iti2);

		std::list<int>::iterator it2a, it2 = list2.begin();
		std::advance(it2,offset);
		it2a = it2;
		++it2a;
		it2 = list2.erase(it2);
		assert(it2a==it2);

		assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );
	}

	// fill the lists
	for (unsigned i=0; i<N; i++) {
		ilist1.push_back( fooNodes[i] );
		ilist2.push_front( fooNodes[i] );
		list1.push_back( fooNodes[i] );
		list2.push_front( fooNodes[i]  );
	}

	// erase random ranges
	while (ilist1.size()) {
		unsigned offset1 = random() % (1+ilist1.size())/2;
		unsigned offset2 = random() % (1+ilist1.size())/2;
		if (offset1>offset2) std::swap(offset1,offset2);
		if (ilist1.size()==1) offset2++;
		
		MCSB::IntrusiveList<Foo>::iterator iti1f, iti1l, iti1;
		std::advance(iti1f=ilist1.begin(), offset1);
		std::advance(iti1l=ilist1.begin(), offset2);
		iti1 = iti1l;
		iti1l = ilist1.erase(iti1f,iti1l);
		assert(iti1l==iti1);

		std::list<int>::iterator it1f, it1l, it1;
		std::advance(it1f=list1.begin(), offset1);
		std::advance(it1l=list1.begin(), offset2);
		it1 = it1l;
		it1l = list1.erase(it1f,it1l);
		assert(it1l==it1);

		assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
	}

	// put lists2 in lists1
	ilist1.insert(ilist1.begin(), ilist2.rbegin(), ilist2.rend());
	list1.insert(list1.begin(), list2.rbegin(), list2.rend());

	assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
	assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );

	// remove by value
	for (unsigned i=0; i<N; i++) {
		unsigned offset = random() % ilist1.size();

		MCSB::IntrusiveList<Foo>::iterator iti1 = ilist1.begin();
		std::advance(iti1,offset);
		//printf("value %d\n", (int)*iti1);
		Foo& foo = fooNodes[*iti1];

		ilist1.remove(foo);
		list1.remove(foo);
		ilist2.remove(foo);
		list2.remove(foo);
		assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
		assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );
	}

	// one last check
	// fill the lists
	for (unsigned i=0; i<N; i++) {
		ilist1.push_back( fooNodes[i] );
		ilist2.push_front( fooNodes[i] );
		list1.push_back( fooNodes[i] );
		list2.push_front( fooNodes[i]  );
	}

	assert( contents_equal(ilist1.begin(),ilist1.end(),list1.begin(),list1.end()) );
	assert( contents_equal(ilist2.begin(),ilist2.end(),list2.begin(),list2.end()) );

	// we also need to be able to create an iterator from a known node
	for (unsigned i=0; i<N; i++) {
		Foo& foo = fooNodes[i];
		// this is all constant time
		MCSB::IntrusiveList<Foo>::iterator it1(&foo);
		ilist1.erase(it1);
		MCSB::IntrusiveList<Foo,FooTag>::iterator it2(&foo);
		ilist2.erase(it2);
	}
	
	assert( ilist1.size()==0 );
	assert( ilist2.size()==0 );

	delete[] fooNodes;

	return 0;
}


//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	unsigned N = 80;
	
	if (argc>1)
		N = atoi(argv[1]);
	
	return test_IntrusiveList(N);
}
