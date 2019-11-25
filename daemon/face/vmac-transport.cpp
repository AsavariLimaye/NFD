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

#include "vmac-transport.hpp"
#include "common/global.hpp"

#include <vmac/vmac.h>

namespace nfd {
namespace face {

NFD_LOG_INIT(VmacTransport);

void vmac_callback(uint8_t type,uint64_t enc, char* buff, uint16_t len, uint16_t seq, char* interestName, uint16_t interestNameLen)
{
  NFD_LOG_INFO("Type: " << type << "  Name: " << interestName << "  Data: " << buff);
}

VmacTransport::VmacTransport()
{
  initVmac();
  this->setLocalUri(FaceUri("vmac://local"));
  this->setRemoteUri(FaceUri("vmac://remote"));
  this->setScope(ndn::nfd::FACE_SCOPE_NON_LOCAL);
  this->setPersistency(ndn::nfd::FACE_PERSISTENCY_PERMANENT);
  this->setLinkType(ndn::nfd::LINK_TYPE_MULTI_ACCESS);
  this->setMtu(MTU_UNLIMITED);
}

void
VmacTransport::doClose()
{
  NFD_LOG_FACE_TRACE(__func__);

  // Ensure that the Transport stays alive at least
  // until all pending handlers are dispatched
  getGlobalIoService().post([this] {
    this->setState(TransportState::CLOSED);
  });
}

void
VmacTransport::doSend(const Block& packet, const EndpointId& endpoint)
{
  NFD_LOG_FACE_TRACE(__func__);
  this->doSend(packet, Name(), endpoint);
}

void
VmacTransport::doSend(const Block& packet, const Name name, const EndpointId& endpoint)
{
  NFD_LOG_FACE_TRACE(__func__);
  NFD_LOG_INFO("Vmac Transport Interest Name" << name);
  this->sendVmac(packet, name);
}

void
VmacTransport::initVmac()
{
  //void (*ptr) (uint8_t a, uint64_t b, char* c, uint16_t d, uint16_t e, char* f, uint16_t g) = &vmac_callback;
  //void (*ptr) (uint8_t a, uint64_t b, char* c, uint16_t d, uint16_t e, char* f, uint16_t g) = &vmacCallback;
  //vmac_register((void*) ptr);
  NFD_LOG_INFO("Vmac Interface Initialized");
}

void
VmacTransport::sendVmac(const Block& packet, const Name name)
{
  NFD_LOG_INFO("Sending VMAC Frame with interest name " << name);
  //send_vmac(0,0,0,"send_data",9,"send_interest",13);
}


void
VmacTransport::vmacCallback(uint8_t type,uint64_t enc, char* buff, uint16_t len, uint16_t seq, char* interestName, uint16_t interestNameLen) {
  NFD_LOG_INFO("Type: " << type << "  Name: " << interestName << "  Data: " << buff);
}

void
VmacTransport::handleError(const std::string& errorMessage)
{
  if (getPersistency() == ndn::nfd::FACE_PERSISTENCY_PERMANENT) {
    NFD_LOG_FACE_DEBUG("Permanent face ignores error: " << errorMessage);
    return;
  }

  NFD_LOG_FACE_ERROR(errorMessage);
  this->setState(TransportState::FAILED);
  doClose();
}

} // namespace face
} // namespace nfd
