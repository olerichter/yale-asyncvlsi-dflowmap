/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2020 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#ifndef DFLOWMAP_COMMON_H
#define DFLOWMAP_COMMON_H

#include <vector>
#include <string>
#include <algorithm>
#include <map>

#define DEBUG_VERBOSE false
#define DEBUG_CLUSTER false
#define DEBUG_FU true

#define MAX_EXPR_TYPE_NUM 100
#define MAX_PROCESSES 500

typedef std::string String;
typedef std::vector<String> StringVec;
typedef std::vector<int> IntVec;
typedef std::vector<const char *> CharPtrVec;

template<class T1, class T2>
using Map = std::map<T1, T2>;

template<class T1, class T2>
std::pair<T1, T2> GenPair(T1 a, T2 b) {
  return std::make_pair(a, b);
}

template<typename A, typename B>
std::pair<B, A> flip_pair(const std::pair<A, B> &p) {
  return std::pair<B, A>(p.second, p.first);
}

template<typename A, typename B>
std::multimap<B, A> flip_map(const std::map<A, B> &src) {
  std::multimap<B, A> dst;
  std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()),
                 flip_pair<A, B>);
  return dst;
}

#endif //DFLOWMAP_COMMON_H
