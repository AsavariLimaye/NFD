/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  Regents of the University of California,
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

#ifndef NFD_DAEMON_FACE_VMAC_FACTORY_HPP
#define NFD_DAEMON_FACE_VMAC_FACTORY_HPP

#include "protocol-factory.hpp"

namespace nfd {
namespace face {

/** \brief Protocol factory for Vmac
 */
class VmacFactory : public ProtocolFactory
{
public:
  static const std::string&
  getId() noexcept;

  explicit
  VmacFactory(const CtorParams& params);

  shared_ptr<Face>
  createMulticastFace();

private:
  /** \brief process face_system.vmac config section
   */
  void
  doProcessConfig(OptionalConfigSection configSection,
                  FaceSystem::ConfigContext& context) override;

  struct VmacConfig
    {
      ssize_t mtu = 2000;
    };
  VmacConfig m_vmacConfig;
  std::map<std::string, shared_ptr<Face>> m_mcastFaces;
};

} // namespace face
} // namespace nfd

#endif // NFD_DAEMON_FACE_VMAC_FACTORY_HPP
