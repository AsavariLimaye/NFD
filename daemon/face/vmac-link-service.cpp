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

#include "vmac-link-service.hpp"

#include <ndn-cxx/lp/pit-token.hpp>
#include <ndn-cxx/lp/tags.hpp>

#include <cmath>

namespace nfd {
namespace face {

NFD_LOG_INIT(VmacLinkService);

constexpr size_t CONGESTION_MARK_SIZE = tlv::sizeOfVarNumber(lp::tlv::CongestionMark) + // type
                                        tlv::sizeOfVarNumber(sizeof(uint64_t)) +        // length
                                        tlv::sizeOfNonNegativeInteger(UINT64_MAX);      // value

VmacLinkService::VmacLinkService(const VmacLinkService::Options& options)
  : m_options(options)
  , m_fragmenter(m_options.fragmenterOptions, this)
  , m_reassembler(m_options.reassemblerOptions, this)
  , m_reliability(m_options.reliabilityOptions, this)
  , m_lastSeqNo(-2)
  , m_nextMarkTime(time::steady_clock::TimePoint::max())
  , m_nMarkedSinceInMarkingState(0)
{
  //m_transport = static_cast<VmacTransport*> (this->getTransport());
  m_reassembler.beforeTimeout.connect([this] (auto...) { ++this->nReassemblyTimeouts; });
  m_reliability.onDroppedInterest.connect([this] (const auto& i) { this->notifyDroppedInterest(i); });
  nReassembling.observe(&m_reassembler);
}

/*
void
VmacLinkService::setFaceAndTransport(Face& face, Transport& transport)
{
  BOOST_ASSERT(m_face == nullptr);
  BOOST_ASSERT(m_transport == nullptr);
  //BOOST_ASSERT(m_vmacTransport == nullptr);

  m_face = &face;
  m_vmacTransport = static_cast<VmacTransport*>(&transport);
  //this->sendPacket(Block(), 0, Name());
}

inline const Transport*
VmacLinkService::getTransport() const
{
	  return m_vmacTransport;
}

inline Transport*
VmacLinkService::getTransport()
{
	  return m_vmacTransport;
}
*/

void
VmacLinkService::setOptions(const VmacLinkService::Options& options)
{
  m_options = options;
  m_fragmenter.setOptions(m_options.fragmenterOptions);
  m_reassembler.setOptions(m_options.reassemblerOptions);
  m_reliability.setOptions(m_options.reliabilityOptions);
}

void
VmacLinkService::requestIdlePacket(const EndpointId& endpointId)
{
  // No need to request Acks to attach to this packet from LpReliability, as they are already
  // attached in sendLpPacket
  this->sendLpPacket({}, endpointId, Name());
}

void
VmacLinkService::sendLpPacket(lp::Packet&& pkt, const EndpointId& endpointId, const Name name)
{
  const ssize_t mtu = this->getTransport()->getMtu();

  if (m_options.reliabilityOptions.isEnabled) {
    m_reliability.piggyback(pkt, mtu);
  }

  if (m_options.allowCongestionMarking) {
    checkCongestionLevel(pkt);
  }

  auto block = pkt.wireEncode();
  if (mtu != MTU_UNLIMITED && block.size() > static_cast<size_t>(mtu)) {
    ++this->nOutOverMtu;
    NFD_LOG_FACE_WARN("attempted to send packet over MTU limit");
    return;
  }
 //this->sendPacket(block, endpointId, name);
}

void
VmacLinkService::doSendInterest(const Interest& interest, const EndpointId& endpointId)
{
  lp::Packet lpPacket(interest.wireEncode());

  encodeLpFields(interest, lpPacket);

  this->sendNetPacket(std::move(lpPacket), endpointId, true, interest.getName());
}

void
VmacLinkService::doSendData(const Data& data, const EndpointId& endpointId)
{
  lp::Packet lpPacket(data.wireEncode());

  encodeLpFields(data, lpPacket);

  this->sendNetPacket(std::move(lpPacket), endpointId, false, data.getName());
}

void
VmacLinkService::doSendNack(const lp::Nack& nack, const EndpointId& endpointId)
{
  lp::Packet lpPacket(nack.getInterest().wireEncode());
  lpPacket.add<lp::NackField>(nack.getHeader());

  encodeLpFields(nack, lpPacket);

  this->sendNetPacket(std::move(lpPacket), endpointId, false, Name());
}

void
VmacLinkService::encodeLpFields(const ndn::PacketBase& netPkt, lp::Packet& lpPacket)
{
  if (m_options.allowLocalFields) {
    auto incomingFaceIdTag = netPkt.getTag<lp::IncomingFaceIdTag>();
    if (incomingFaceIdTag != nullptr) {
      lpPacket.add<lp::IncomingFaceIdField>(*incomingFaceIdTag);
    }
  }

  auto congestionMarkTag = netPkt.getTag<lp::CongestionMarkTag>();
  if (congestionMarkTag != nullptr) {
    lpPacket.add<lp::CongestionMarkField>(*congestionMarkTag);
  }

  if (m_options.allowSelfLearning) {
    auto nonDiscoveryTag = netPkt.getTag<lp::NonDiscoveryTag>();
    if (nonDiscoveryTag != nullptr) {
      lpPacket.add<lp::NonDiscoveryField>(*nonDiscoveryTag);
    }

    auto prefixAnnouncementTag = netPkt.getTag<lp::PrefixAnnouncementTag>();
    if (prefixAnnouncementTag != nullptr) {
      lpPacket.add<lp::PrefixAnnouncementField>(*prefixAnnouncementTag);
    }
  }

  auto pitToken = netPkt.getTag<lp::PitToken>();
  if (pitToken != nullptr) {
    lpPacket.add<lp::PitTokenField>(*pitToken);
  }
}

void
VmacLinkService::sendNetPacket(lp::Packet&& pkt, const EndpointId& endpointId, bool isInterest, const Name name)
{
  NFD_LOG_INFO("Sending interest with name: " << name);

  std::vector<lp::Packet> frags;
  ssize_t mtu = this->getTransport()->getMtu();

  // Make space for feature fields in fragments
  if (m_options.reliabilityOptions.isEnabled && mtu != MTU_UNLIMITED) {
    mtu -= VmacReliability::RESERVED_HEADER_SPACE;
  }

  if (m_options.allowCongestionMarking && mtu != MTU_UNLIMITED) {
    mtu -= CONGESTION_MARK_SIZE;
  }

  BOOST_ASSERT(mtu == MTU_UNLIMITED || mtu > 0);

  if (m_options.allowFragmentation && mtu != MTU_UNLIMITED) {
    bool isOk = false;
    std::tie(isOk, frags) = m_fragmenter.fragmentPacket(pkt, mtu);
    if (!isOk) {
      // fragmentation failed (warning is logged by LpFragmenter)
      ++this->nFragmentationErrors;
      return;
    }
  }
  else {
    if (m_options.reliabilityOptions.isEnabled) {
      frags.push_back(pkt);
    }
    else {
      frags.push_back(std::move(pkt));
    }
  }

  if (frags.size() == 1) {
    // even if indexed fragmentation is enabled, the fragmenter should not
    // fragment the packet if it can fit in MTU
    BOOST_ASSERT(!frags.front().has<lp::FragIndexField>());
    BOOST_ASSERT(!frags.front().has<lp::FragCountField>());
  }

  // Only assign sequences to fragments if packet contains more than 1 fragment
  if (frags.size() > 1) {
    // Assign sequences to all fragments
    this->assignSequences(frags);
  }

  if (m_options.reliabilityOptions.isEnabled && frags.front().has<lp::FragmentField>()) {
    m_reliability.handleOutgoing(frags, std::move(pkt), isInterest, name);
  }

  for (lp::Packet& frag : frags) {
    this->sendLpPacket(std::move(frag), endpointId, name);
  }
}

void
VmacLinkService::assignSequence(lp::Packet& pkt)
{
  pkt.set<lp::SequenceField>(++m_lastSeqNo);
}

void
VmacLinkService::assignSequences(std::vector<lp::Packet>& pkts)
{
  std::for_each(pkts.begin(), pkts.end(), [this] (auto& pkt) { this->assignSequence(pkt); });
}

void
VmacLinkService::checkCongestionLevel(lp::Packet& pkt)
{
  ssize_t sendQueueLength = getTransport()->getSendQueueLength();
  // The transport must support retrieving the current send queue length
  if (sendQueueLength < 0) {
    return;
  }

  if (sendQueueLength > 0) {
    NFD_LOG_FACE_TRACE("txqlen=" << sendQueueLength << " threshold=" <<
                       m_options.defaultCongestionThreshold << " capacity=" <<
                       getTransport()->getSendQueueCapacity());
  }

  // sendQueue is above target
  if (static_cast<size_t>(sendQueueLength) > m_options.defaultCongestionThreshold) {
    const auto now = time::steady_clock::now();

    if (m_nextMarkTime == time::steady_clock::TimePoint::max()) {
      m_nextMarkTime = now + m_options.baseCongestionMarkingInterval;
    }
    // Mark packet if sendQueue stays above target for one interval
    else if (now >= m_nextMarkTime) {
      pkt.set<lp::CongestionMarkField>(1);
      ++nCongestionMarked;
      NFD_LOG_FACE_DEBUG("LpPacket was marked as congested");

      ++m_nMarkedSinceInMarkingState;
      // Decrease the marking interval by the inverse of the square root of the number of packets
      // marked in this incident of congestion
      time::nanoseconds interval(static_cast<time::nanoseconds::rep>(
                                   m_options.baseCongestionMarkingInterval.count() /
                                   std::sqrt(m_nMarkedSinceInMarkingState + 1)));
      m_nextMarkTime += interval;
    }
  }
  else if (m_nextMarkTime != time::steady_clock::TimePoint::max()) {
    // Congestion incident has ended, so reset
    NFD_LOG_FACE_DEBUG("Send queue length dropped below congestion threshold");
    m_nextMarkTime = time::steady_clock::TimePoint::max();
    m_nMarkedSinceInMarkingState = 0;
  }
}

void
VmacLinkService::doReceivePacket(const Block& packet, const EndpointId& endpoint)
{
  try {
    lp::Packet pkt(packet);

    if (m_options.reliabilityOptions.isEnabled) {
      m_reliability.processIncomingPacket(pkt, Name());
    }

    if (!pkt.has<lp::FragmentField>()) {
      NFD_LOG_FACE_TRACE("received IDLE packet: DROP");
      return;
    }

    if ((pkt.has<lp::FragIndexField>() || pkt.has<lp::FragCountField>()) &&
        !m_options.allowReassembly) {
      NFD_LOG_FACE_WARN("received fragment, but reassembly disabled: DROP");
      return;
    }

    bool isReassembled = false;
    Block netPkt;
    lp::Packet firstPkt;
    std::tie(isReassembled, netPkt, firstPkt) = m_reassembler.receiveFragment(endpoint, pkt);
    if (isReassembled) {
      this->decodeNetPacket(netPkt, firstPkt, endpoint);
    }
  }
  catch (const tlv::Error& e) {
    ++this->nInLpInvalid;
    NFD_LOG_FACE_WARN("packet parse error (" << e.what() << "): DROP");
  }
}

void
VmacLinkService::decodeNetPacket(const Block& netPkt, const lp::Packet& firstPkt,
                                    const EndpointId& endpointId)
{
  try {
    switch (netPkt.type()) {
      case tlv::Interest:
        if (firstPkt.has<lp::NackField>()) {
          this->decodeNack(netPkt, firstPkt, endpointId);
        }
        else {
          this->decodeInterest(netPkt, firstPkt, endpointId);
        }
        break;
      case tlv::Data:
        this->decodeData(netPkt, firstPkt, endpointId);
        break;
      default:
        ++this->nInNetInvalid;
        NFD_LOG_FACE_WARN("unrecognized network-layer packet TLV-TYPE " << netPkt.type() << ": DROP");
        return;
    }
  }
  catch (const tlv::Error& e) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("packet parse error (" << e.what() << "): DROP");
  }
}

void
VmacLinkService::decodeInterest(const Block& netPkt, const lp::Packet& firstPkt,
                                   const EndpointId& endpointId)
{
  BOOST_ASSERT(netPkt.type() == tlv::Interest);
  BOOST_ASSERT(!firstPkt.has<lp::NackField>());

  // forwarding expects Interest to be created with make_shared
  auto interest = make_shared<Interest>(netPkt);

  if (firstPkt.has<lp::NextHopFaceIdField>()) {
    if (m_options.allowLocalFields) {
      interest->setTag(make_shared<lp::NextHopFaceIdTag>(firstPkt.get<lp::NextHopFaceIdField>()));
    }
    else {
      NFD_LOG_FACE_WARN("received NextHopFaceId, but local fields disabled: DROP");
      return;
    }
  }

  if (firstPkt.has<lp::CachePolicyField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received CachePolicy with Interest: DROP");
    return;
  }

  if (firstPkt.has<lp::IncomingFaceIdField>()) {
    NFD_LOG_FACE_WARN("received IncomingFaceId: IGNORE");
  }

  if (firstPkt.has<lp::CongestionMarkField>()) {
    interest->setTag(make_shared<lp::CongestionMarkTag>(firstPkt.get<lp::CongestionMarkField>()));
  }

  if (firstPkt.has<lp::NonDiscoveryField>()) {
    if (m_options.allowSelfLearning) {
      interest->setTag(make_shared<lp::NonDiscoveryTag>(firstPkt.get<lp::NonDiscoveryField>()));
    }
    else {
      NFD_LOG_FACE_WARN("received NonDiscovery, but self-learning disabled: IGNORE");
    }
  }

  if (firstPkt.has<lp::PrefixAnnouncementField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received PrefixAnnouncement with Interest: DROP");
    return;
  }

  if (firstPkt.has<lp::PitTokenField>()) {
    interest->setTag(make_shared<lp::PitToken>(firstPkt.get<lp::PitTokenField>()));
  }

  this->receiveInterest(*interest, endpointId);
}

void
VmacLinkService::decodeData(const Block& netPkt, const lp::Packet& firstPkt,
                               const EndpointId& endpointId)
{
  BOOST_ASSERT(netPkt.type() == tlv::Data);

  // forwarding expects Data to be created with make_shared
  auto data = make_shared<Data>(netPkt);

  if (firstPkt.has<lp::NackField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received Nack with Data: DROP");
    return;
  }

  if (firstPkt.has<lp::NextHopFaceIdField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received NextHopFaceId with Data: DROP");
    return;
  }

  if (firstPkt.has<lp::CachePolicyField>()) {
    // CachePolicy is unprivileged and does not require allowLocalFields option.
    // In case of an invalid CachePolicyType, get<lp::CachePolicyField> will throw,
    // so it's unnecessary to check here.
    data->setTag(make_shared<lp::CachePolicyTag>(firstPkt.get<lp::CachePolicyField>()));
  }

  if (firstPkt.has<lp::IncomingFaceIdField>()) {
    NFD_LOG_FACE_WARN("received IncomingFaceId: IGNORE");
  }

  if (firstPkt.has<lp::CongestionMarkField>()) {
    data->setTag(make_shared<lp::CongestionMarkTag>(firstPkt.get<lp::CongestionMarkField>()));
  }

  if (firstPkt.has<lp::NonDiscoveryField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received NonDiscovery with Data: DROP");
    return;
  }

  if (firstPkt.has<lp::PrefixAnnouncementField>()) {
    if (m_options.allowSelfLearning) {
      data->setTag(make_shared<lp::PrefixAnnouncementTag>(firstPkt.get<lp::PrefixAnnouncementField>()));
    }
    else {
      NFD_LOG_FACE_WARN("received PrefixAnnouncement, but self-learning disabled: IGNORE");
    }
  }

  this->receiveData(*data, endpointId);
}

void
VmacLinkService::decodeNack(const Block& netPkt, const lp::Packet& firstPkt,
                               const EndpointId& endpointId)
{
  BOOST_ASSERT(netPkt.type() == tlv::Interest);
  BOOST_ASSERT(firstPkt.has<lp::NackField>());

  lp::Nack nack((Interest(netPkt)));
  nack.setHeader(firstPkt.get<lp::NackField>());

  if (firstPkt.has<lp::NextHopFaceIdField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received NextHopFaceId with Nack: DROP");
    return;
  }

  if (firstPkt.has<lp::CachePolicyField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received CachePolicy with Nack: DROP");
    return;
  }

  if (firstPkt.has<lp::IncomingFaceIdField>()) {
    NFD_LOG_FACE_WARN("received IncomingFaceId: IGNORE");
  }

  if (firstPkt.has<lp::CongestionMarkField>()) {
    nack.setTag(make_shared<lp::CongestionMarkTag>(firstPkt.get<lp::CongestionMarkField>()));
  }

  if (firstPkt.has<lp::NonDiscoveryField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received NonDiscovery with Nack: DROP");
    return;
  }

  if (firstPkt.has<lp::PrefixAnnouncementField>()) {
    ++this->nInNetInvalid;
    NFD_LOG_FACE_WARN("received PrefixAnnouncement with Nack: DROP");
    return;
  }

  this->receiveNack(nack, endpointId);
}

} // namespace face
} // namespace nfd
