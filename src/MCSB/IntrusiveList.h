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

#ifndef MCSB_IntrusiveList_h
#define MCSB_IntrusiveList_h
#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <iterator>

namespace MCSB {

//-----------------------------------------------------------------------------
// IntrusiveList is similar to boost::intrusive::list<T,tag> and
// generally uses the same semantics as boost::intrusive::list or std::list
//-----------------------------------------------------------------------------

// this is just a forward declaration; the definition is below
template<typename T, typename tag=T> class IntrusiveList;

//-----------------------------------------------------------------------------
// similar to boost::intrusive::list_base_hook<tag>
// inherit from this class, but never manipulate it directly
template<typename tag>
class IntrusiveListHook {
  public:
	IntrusiveListHook(void): next_(this), prev_(this) {}

  private:
	IntrusiveListHook* next_;
	IntrusiveListHook* prev_;

	void insert(IntrusiveListHook* node) { // before
		assert( detached() ); // not in any list
		next_ = node;
		prev_ = node->prev_;
		node->prev_ = prev_->next_ = this;
	}
	void insert_after(IntrusiveListHook* node) { insert(node->next_); }
	void remove(void) {
		assert( !detached() ); // in some list
		next_->prev_ = prev_;
		prev_->next_ = next_;
		prev_ = next_ = this;
	}
	bool detached(void) const { return next_ == this; }

	template<typename T, typename ttag> friend class IntrusiveList;
	template<typename T, typename ttag> friend class _IntrusiveList_iterator;
	template<typename T, typename ttag> friend class _IntrusiveList_const_iterator;

	// disallowed
	IntrusiveListHook(const IntrusiveListHook&);
	IntrusiveListHook& operator=(const IntrusiveListHook&);
};


//-----------------------------------------------------------------------------
// not for direct use -- use IntrusiveList<>::iterator
template<typename T, typename tag>
class _IntrusiveList_iterator {
	typedef IntrusiveListHook<tag> Hook;
	Hook* node_;
  public:
	typedef _IntrusiveList_iterator<T,tag> _Self;

	// auto copy ctor and op=
	explicit _IntrusiveList_iterator(Hook* p=0): node_(p) {}
	T& operator*() const { return *static_cast<T*>(node_); }
	T* operator->() const { return static_cast<T*>(node_); }
	_Self& operator++() { node_ = node_->next_; return *this; }
	_Self& operator--() { node_ = node_->prev_; return *this; }
	_Self operator++(int) { _Self r(*this); node_ = node_->next_; return r; }
	_Self operator--(int) { _Self r(*this); node_ = node_->prev_; return r; }
	bool operator==(const _Self& rhs) const { return node_ == rhs.node_; }
	bool operator!=(const _Self& rhs) const { return node_ != rhs.node_; }
	template<typename tT, typename ttag> friend class _IntrusiveList_const_iterator;

	// support for std::reverse_iterator
	typedef std::bidirectional_iterator_tag  iterator_category;
	typedef ptrdiff_t                        difference_type;
	typedef T                                value_type;
	typedef T*                               pointer;
	typedef T&                               reference;
};


//-----------------------------------------------------------------------------
// not for direct use -- use IntrusiveList<>::const_iterator
template<typename T, typename tag>
class _IntrusiveList_const_iterator {
	typedef IntrusiveListHook<tag> Hook;
	const Hook* node_;
  public:
	typedef _IntrusiveList_const_iterator<T,tag> _Self;
	typedef _IntrusiveList_iterator<T,tag>       iterator;

	// auto copy ctor and op=
	explicit _IntrusiveList_const_iterator(const Hook* p=0): node_(p) {}
	_IntrusiveList_const_iterator(const iterator& it): node_(it.node_) {}
	const T& operator*() const { return *static_cast<const T*>(node_); }
	const T* operator->() const { return static_cast<const T*>(node_); }
	_Self& operator++() { node_ = node_->next_; return *this; }
	_Self& operator--() { node_ = node_->prev_; return *this; }
	_Self operator++(int) { _Self r(*this); node_ = node_->next_; return r; }
	_Self operator--(int) { _Self r(*this); node_ = node_->prev_; return r; }
	bool operator==(const _Self& rhs) const { return node_ == rhs.node_; }
	bool operator!=(const _Self& rhs) const { return node_ != rhs.node_; }

	// support for std::const_reverse_iterator
	typedef std::bidirectional_iterator_tag  iterator_category;
	typedef ptrdiff_t                        difference_type;
	typedef T                                value_type;
	typedef T*                               pointer;
	typedef T&                               reference;
};


//-----------------------------------------------------------------------------
// comparison of iterator and const_iterator
template<typename T, typename tag>
inline bool operator==(const _IntrusiveList_iterator<T,tag>& x,
	const _IntrusiveList_iterator<T,tag>& y)
{	return x.node_ == y.node_; }

template<typename T, typename tag>
inline bool operator!=(const _IntrusiveList_iterator<T,tag>& x,
	const _IntrusiveList_iterator<T,tag>& y)
{	return x.node_ != y.node_; }


//-----------------------------------------------------------------------------
// similar to boost::intrusive::list<T,tag>
template<typename T, typename tag>
class IntrusiveList {
  public:
	IntrusiveList(void): size_(0) {}
   ~IntrusiveList(void) { while(!empty()) pop_back(); }

	bool empty(void) const { return root_.next_ == &root_; }
	size_t size(void) const { return size_; }

	T& front(void) { return *static_cast<T*>(root_.next_); }
	T& back(void)  { return *static_cast<T*>(root_.prev_); }
	const T& front(void) const { return *static_cast<const T*>(root_.next_); }
	const T& back(void) const  { return *static_cast<const T*>(root_.prev_); }

	void push_front(T& t) { asHook(t)->insert_after(&root_); ++size_; }
	void push_back(T& t)  { asHook(t)->insert(&root_); ++size_; }

	void pop_front(void) { asHook(front())->remove(); --size_; }
	void pop_back(void)  { asHook(back())->remove(); --size_; }

	typedef _IntrusiveList_iterator<T,tag>          iterator;
	typedef _IntrusiveList_const_iterator<T,tag>    const_iterator;
	typedef std::reverse_iterator<iterator>         reverse_iterator;
	typedef std::reverse_iterator<const_iterator>   const_reverse_iterator;

	iterator begin(void) { return iterator( asHook(front()) ); }
	iterator end(void) { return iterator(&root_); }

	const_iterator begin(void) const { return const_iterator( asHook(front()) ); }
	const_iterator end(void) const { return const_iterator(&root_); }

	reverse_iterator rbegin(void) { return reverse_iterator( end() ); }
	reverse_iterator rend(void) { return reverse_iterator( begin() ); }

	const_reverse_iterator rbegin(void) const { return const_reverse_iterator( end() ); }
	const_reverse_iterator rend(void) const { return const_reverse_iterator( begin() ); }

	// insert before iterator p
	iterator insert(iterator p, T& t) {
		Hook* in = asHook(t);
		in->insert( asHook(*p) );
		++size_;
		return iterator(in);
	}
	template<typename Iter>
	void insert(iterator p, Iter b, Iter e) { for (; b!=e; ++b) insert(p,*b); }

	// erase at iterator(s)
	iterator erase(iterator p) { asHook(*p++)->remove(); --size_; return p; }
	iterator erase(iterator f, iterator l) {
		while (f != l) { asHook(*f++)->remove(); --size_; }
		return f;
	}

	// remove by value
	void remove(const T& t) { // TODO: check me
		iterator it = begin();
		iterator last = end();
		while (it!=last) {
			if (*it == t) it = erase(it);
			else ++it;
		}
	}
	
	typedef IntrusiveListHook<tag> Hook;
	iterator erase(Hook& h) { return erase(iterator(&h)); }

  private:
	Hook root_;
	size_t size_;
	
	static Hook* asHook(T& t) { return static_cast<Hook*>(&t); }
	static const Hook* asHook(const T& t) { return static_cast<const Hook*>(&t); }
};


} // namespace MCSB

#endif
