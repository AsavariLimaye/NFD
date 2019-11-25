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

#include "vmac-factory.hpp"
#include "vmac-link-service.hpp"
#include "vmac-transport.hpp"

namespace nfd {
namespace face {

NFD_LOG_INIT(VmacFactory);
NFD_REGISTER_PROTOCOL_FACTORY(VmacFactory);

const std::string&
VmacFactory::getId() noexcept
{
  static std::string id("vmac");
  return id;
}

VmacFactory::VmacFactory(const CtorParams& params)
  : ProtocolFactory(params)
{
  NFD_LOG_INFO("Creating multicast vmac face");

  shared_ptr<Face> face;
  face = this->createMulticastFace();

  if (face->getId() == face::INVALID_FACEID) {
    // new face: register with forwarding
    this->addFace(face);
  }
}

void
VmacFactory::doProcessConfig(OptionalConfigSection configSection,
                                 FaceSystem::ConfigContext& context)
{

  if (configSection) {

    for (const auto& pair : *configSection) {
      const std::string& key = pair.first;
      const ConfigSection& value = pair.second;

      if (key == "vmackey") {
        
      }
      else {
        NDN_THROW(ConfigFile::Error("Unrecognized option face_system.ether." + key));
      }
    }
  }

  if (context.isDryRun) {
    return;
  }

}

shared_ptr<Face>
VmacFactory::createMulticastFace()
{
  auto key = std::move("vmac");
  VmacLinkService::Options opts;
  opts.allowFragmentation = true;
  opts.allowReassembly = true;

  auto linkService = make_unique<VmacLinkService>(opts);
  auto transport = make_unique<VmacTransport>();
  auto face = make_shared<Face>(std::move(linkService), std::move(transport));

  m_mcastFaces[key] = face;
  connectFaceClosedSignal(*face, [this, key] { m_mcastFaces.erase(key); });

  return face;
}

} // namespace face
} // namespace nfd