/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  Regents of the University of California,
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

#ifndef NFD_DAEMON_FACE_VMAC_TRANSPORT_HPP
#define NFD_DAEMON_FACE_VMAC_TRANSPORT_HPP

#include "transport.hpp"
#include "vmac-interface.hpp"

namespace nfd {
namespace face {

/**
 * @brief Cass for Vmac Transport
 */
class VmacTransport : public Transport
{
public:
  class Error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  VmacTransport();

protected:
  void
  doClose() final;

  bool
  hasRecentlyReceived() const
  {
    return m_hasRecentlyReceived;
  }

  void
  resetRecentlyReceived()
  {
    m_hasRecentlyReceived = false;
  }

private:

  void
  doSend(const Block& packet, const EndpointId& endpoint) final;

  /**
   * @brief Sends the specified TLV block on the network wrapped in an VMAC frame
   */
  void
  sendPacket(const ndn::Block& block);

  void
  handleError(const std::string& errorMessage);

private:
  bool m_hasRecentlyReceived;
  VmacInterface m_vmacInterface;
#ifdef _DEBUG
  size_t m_nDropped;
#endif
};

} // namespace face
} // namespace nfd

#endif // NFD_DAEMON_FACE_ETHERNET_TRANSPORT_HPP
