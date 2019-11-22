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

  void
  send(const Block& packet, const EndpointId& endpoint);

  void
  send(const Block& packet, const EndpointId& endpoint, const Name name);

protected:
  void
  doClose() OVERRIDE_WITH_TESTS_ELSE_FINAL;
private:

  void
  doSend(const Block& packet, const EndpointId& endpoint) OVERRIDE_WITH_TESTS_ELSE_FINAL;

  void
  doSend(const Block& packet, const EndpointId& endpoint, const Name name);

  /**
   * @brief Sends the specified TLV block on the network wrapped in an VMAC frame
   */
  void
  handleError(const std::string& errorMessage);

private:
  void initVmac();
  void sendVmac(const Block& packet, const Name name);
  void vmacCallback(uint8_t type,uint64_t enc, char* buff, uint16_t len, uint16_t seq, char* interestName, uint16_t interestNameLen);
};

} // namespace face
} // namespace nfd

#endif // NFD_DAEMON_FACE_VMAC_TRANSPORT_HPP
