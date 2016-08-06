/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "name-tree.hpp"
#include "core/logger.hpp"

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <type_traits>

namespace nfd {
namespace name_tree {

NFD_LOG_INIT("NameTree");

NameTree::NameTree(size_t nBuckets)
  : m_ht(HashtableOptions(nBuckets))
{
}

shared_ptr<Entry>
NameTree::lookup(const Name& name)
{
  NFD_LOG_TRACE("lookup " << name);

  HashSequence hashes = computeHashes(name);
  const Node* node = nullptr;
  Entry* parent = nullptr;

  for (size_t prefixLen = 0; prefixLen <= name.size(); ++prefixLen) {
    bool isNew = false;
    std::tie(node, isNew) = m_ht.insert(name, prefixLen, hashes);

    if (isNew && parent != nullptr) {
      node->entry->setParent(*parent);
    }
    parent = node->entry.get();
  }
  return node->entry;
}

shared_ptr<Entry>
NameTree::lookup(const fib::Entry& fibEntry) const
{
  shared_ptr<Entry> nte = this->getEntry(fibEntry);
  BOOST_ASSERT(nte == nullptr || nte->getFibEntry() == &fibEntry);
  return nte;
}

shared_ptr<Entry>
NameTree::lookup(const pit::Entry& pitEntry)
{
  shared_ptr<Entry> nte = this->getEntry(pitEntry);
  if (nte == nullptr) {
    return nullptr;
  }

  if (nte->getPrefix().size() == pitEntry.getName().size()) {
    return nte;
  }

  BOOST_ASSERT(pitEntry.getName().at(-1).isImplicitSha256Digest());
  BOOST_ASSERT(nte->getPrefix() == pitEntry.getName().getPrefix(-1));
  return this->lookup(pitEntry.getName());
}

shared_ptr<Entry>
NameTree::lookup(const measurements::Entry& measurementsEntry) const
{
  shared_ptr<Entry> nte = this->getEntry(measurementsEntry);
  BOOST_ASSERT(nte == nullptr || nte->getMeasurementsEntry() == &measurementsEntry);
  return nte;
}

shared_ptr<Entry>
NameTree::lookup(const strategy_choice::Entry& strategyChoiceEntry) const
{
  shared_ptr<Entry> nte = this->getEntry(strategyChoiceEntry);
  BOOST_ASSERT(nte == nullptr || nte->getStrategyChoiceEntry() == &strategyChoiceEntry);
  return nte;
}

size_t
NameTree::eraseEntryIfEmpty(Entry* entry)
{
  BOOST_ASSERT(entry != nullptr);

  size_t nErased = 0;
  for (Entry* parent = nullptr; entry != nullptr && entry->isEmpty(); entry = parent) {
    parent = entry->getParent();

    if (parent != nullptr) {
      entry->unsetParent();
    }

    m_ht.erase(getNode(*entry));
    ++nErased;
  }

  if (nErased == 0) {
    NFD_LOG_TRACE("not-erase " << entry->getName());
  }
  return nErased;
}

shared_ptr<Entry>
NameTree::findExactMatch(const Name& name) const
{
  const Node* node = m_ht.find(name, name.size());
  return node == nullptr ? nullptr : node->entry;
}

// Longest Prefix Match
shared_ptr<Entry>
NameTree::findLongestPrefixMatch(const Name& name, const EntrySelector& entrySelector) const
{
  HashSequence hashes = computeHashes(name);

  for (ssize_t prefixLen = name.size(); prefixLen >= 0; --prefixLen) {
    const Node* node = m_ht.find(name, prefixLen, hashes);
    if (node != nullptr && entrySelector(*node->entry)) {
      return node->entry;
    }
  }

  return nullptr;
}

shared_ptr<Entry>
NameTree::findLongestPrefixMatch(shared_ptr<Entry> entry1,
                                 const EntrySelector& entrySelector) const
{
  Entry* entry = entry1.get();
  while (entry != nullptr) {
    if (entrySelector(*entry)) {
      return entry->shared_from_this();
    }
    entry = entry->getParent();
  }
  return nullptr;
}

shared_ptr<Entry>
NameTree::findLongestPrefixMatch(const pit::Entry& pitEntry) const
{
  shared_ptr<Entry> nte = this->getEntry(pitEntry);
  BOOST_ASSERT(nte != nullptr);
  if (nte->getPrefix().size() == pitEntry.getName().size()) {
    return nte;
  }

  BOOST_ASSERT(pitEntry.getName().at(-1).isImplicitSha256Digest());
  BOOST_ASSERT(nte->getPrefix() == pitEntry.getName().getPrefix(-1));
  shared_ptr<Entry> exact = this->findExactMatch(pitEntry.getName());
  return exact == nullptr ? nte : exact;
}

boost::iterator_range<NameTree::const_iterator>
NameTree::findAllMatches(const Name& prefix,
                         const EntrySelector& entrySelector) const
{
  NFD_LOG_TRACE("NameTree::findAllMatches" << prefix);

  // As we are using Name Prefix Hash Table, and the current LPM() is
  // implemented as starting from full name, and reduce the number of
  // components by 1 each time, we could use it here.
  // For trie-like design, it could be more efficient by walking down the
  // trie from the root node.

  shared_ptr<Entry> entry = findLongestPrefixMatch(prefix, entrySelector);
  return {Iterator(make_shared<PrefixMatchImpl>(*this, entrySelector), entry.get()), end()};
}

boost::iterator_range<NameTree::const_iterator>
NameTree::fullEnumerate(const EntrySelector& entrySelector) const
{
  NFD_LOG_TRACE("fullEnumerate");

  return {Iterator(make_shared<FullEnumerationImpl>(*this, entrySelector), nullptr), end()};
}

boost::iterator_range<NameTree::const_iterator>
NameTree::partialEnumerate(const Name& prefix,
                           const EntrySubTreeSelector& entrySubTreeSelector) const
{
  // the first step is to process the root node
  shared_ptr<Entry> entry = findExactMatch(prefix);
  return {Iterator(make_shared<PartialEnumerationImpl>(*this, entrySubTreeSelector), entry.get()), end()};
}

} // namespace name_tree
} // namespace nfd
