/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef COMMON_QUEUE_H
#define COMMON_QUEUE_H

#include "common/scummsys.h"
#include "common/list.h"

namespace Common {

/**
 * @defgroup common_queue Queue
 * @ingroup common
 *
 * @brief API and templates for queues.
 * @{
 */

/**
 * Variable size Queue class, implemented using our List class.
 */
template<class T>
class Queue {
//public:
//	typedef T value_type;

public:
	bool empty() const {
		return _impl.empty();
	}

	void clear() {
		_impl.clear();
	}

#ifndef SCUMMVM_CXX03_COMPAT
	template<class... TArgs>
	void emplace(TArgs&&... args) {
		_impl.emplace_back(Common::forward<TArgs>(args)...);
	}
#else
	
	void emplace() {
		_impl.emplace_back();
	}
	template<class A1>
	void emplace(A1 &&a1) {
		_impl.emplace_back(Common::forward<A1>(a1));
	}
	template<class A1, class A2>
	void emplace(A1 &&a1, A2 &&a2) {
		_impl.emplace_back(Common::forward<A1>(a1), Common::forward<A2>(a2));
	}
	template<class A1, class A2, class A3>
	void emplace(A1 &&a1, A2 &&a2, A3 &&a3) {
		_impl.emplace_back(Common::forward<A1>(a1), Common::forward<A2>(a2), Common::forward<A3>(a3));
	}
	template<class A1, class A2, class A3, class A4>
	void emplace(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4) {
		_impl.emplace_back(Common::forward<A1>(a1), Common::forward<A2>(a2), Common::forward<A3>(a3), Common::forward<A4>(a4));
	}
	template<class A1, class A2, class A3, class A4, class A5>
	void emplace(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5) {
		_impl.emplace_back(Common::forward<A1>(a1), Common::forward<A2>(a2), Common::forward<A3>(a3), Common::forward<A4>(a4), Common::forward<A5>(a5));
	}
	template<class A1, class A2, class A3, class A4, class A5, class A6>
	void emplace(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5, A6 &&a6) {
		_impl.emplace_back(Common::forward<A1>(a1), Common::forward<A2>(a2), Common::forward<A3>(a3), Common::forward<A4>(a4), Common::forward<A5>(a5), Common::forward<A6>(a6));
	}
#endif

	void push(const T &x) {
		_impl.push_back(x);
	}

	void push(T &&x) {
		_impl.push_back(Common::move(x));
	}

	T &front() {
		return _impl.front();
	}

	const T &front() const {
		return _impl.front();
	}

	T &back() {
		return _impl.back();
	}

	const T &back() const {
		return _impl.back();
	}

	T pop() {
		T tmp = Common::move(front());
		_impl.pop_front();
		return tmp;
	}

	int size() const {
		return _impl.size();
	}

private:
	List<T>	_impl;
};

/** @} */

} // End of namespace Common

#endif
