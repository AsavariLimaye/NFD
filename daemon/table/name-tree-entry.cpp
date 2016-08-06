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

#include "name-tree-entry.hpp"

namespace nfd {
namespace name_tree {

Entry::Entry(const Name& name, Node* node)
  : m_name(name)
  , m_node(node)
  , m_parent(nullptr)
{
  BOOST_ASSERT(node != nullptr);
}

void
Entry::setParent(Entry& entry)
{
  BOOST_ASSERT(this->getParent() == nullptr);
  BOOST_ASSERT(!this->getName().empty());
  BOOST_ASSERT(entry.getName() == this->getName().getPrefix(-1));

  m_parent = &entry;

  m_parent->m_children.push_back(this);
}

void
Entry::unsetParent()
{
  BOOST_ASSERT(this->getParent() != nullptr);

  auto i = std::find(m_parent->m_children.begin(), m_parent->m_children.end(), this);
  BOOST_ASSERT(i != m_parent->m_children.end());
  m_parent->m_children.erase(i);

  m_parent = nullptr;
}

bool
Entry::hasTableEntries() const
{
  return m_fibEntry != nullptr ||
         !m_pitEntries.empty() ||
         m_measurementsEntry != nullptr ||
         m_strategyChoiceEntry != nullptr;
}

void
Entry::setFibEntry(unique_ptr<fib::Entry> fibEntry)
{
  BOOST_ASSERT(fibEntry == nullptr || fibEntry->m_nameTreeEntry.expired());

  if (m_fibEntry != nullptr) {
    m_fibEntry->m_nameTreeEntry.reset();
  }
  m_fibEntry = std::move(fibEntry);

  if (m_fibEntry != nullptr) {
    m_fibEntry->m_nameTreeEntry = this->shared_from_this();
  }
}

void
Entry::insertPitEntry(shared_ptr<pit::Entry> pitEntry)
{
  BOOST_ASSERT(pitEntry != nullptr);
  BOOST_ASSERT(pitEntry->m_nameTreeEntry.expired());

  m_pitEntries.push_back(pitEntry);
  pitEntry->m_nameTreeEntry = this->shared_from_this();
}

void
Entry::erasePitEntry(shared_ptr<pit::Entry> pitEntry)
{
  BOOST_ASSERT(pitEntry != nullptr);
  BOOST_ASSERT(pitEntry->m_nameTreeEntry.lock().get() == this);

  auto it = std::find(m_pitEntries.begin(), m_pitEntries.end(), pitEntry);
  BOOST_ASSERT(it != m_pitEntries.end());

  *it = m_pitEntries.back();
  m_pitEntries.pop_back();
  pitEntry->m_nameTreeEntry.reset();
}

void
Entry::setMeasurementsEntry(unique_ptr<measurements::Entry> measurementsEntry)
{
  BOOST_ASSERT(measurementsEntry == nullptr || measurementsEntry->m_nameTreeEntry.expired());

  if (m_measurementsEntry != nullptr) {
    m_measurementsEntry->m_nameTreeEntry.reset();
  }
  m_measurementsEntry = std::move(measurementsEntry);

  if (m_measurementsEntry != nullptr) {
    m_measurementsEntry->m_nameTreeEntry = this->shared_from_this();
  }
}

void
Entry::setStrategyChoiceEntry(unique_ptr<strategy_choice::Entry> strategyChoiceEntry)
{
  BOOST_ASSERT(strategyChoiceEntry == nullptr || strategyChoiceEntry->m_nameTreeEntry.expired());

  if (m_strategyChoiceEntry != nullptr) {
    m_strategyChoiceEntry->m_nameTreeEntry.reset();
  }
  m_strategyChoiceEntry = std::move(strategyChoiceEntry);

  if (m_strategyChoiceEntry != nullptr) {
    m_strategyChoiceEntry->m_nameTreeEntry = this->shared_from_this();
  }
}

} // namespace name_tree
} // namespace nfd
