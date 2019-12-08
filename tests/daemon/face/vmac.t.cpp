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

#include "face/vmac-link-service.hpp"
#include "face/vmac-transport.hpp"
#include "face/face.hpp"

#include "tests/test-common.hpp"
#include "tests/key-chain-fixture.hpp"
#include "tests/daemon/global-io-fixture.hpp"

#include <ndn-cxx/lp/empty-value.hpp>
#include <ndn-cxx/lp/prefix-announcement-header.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>

namespace nfd {
namespace face {
namespace tests {

using namespace nfd::tests;

BOOST_AUTO_TEST_SUITE(Face)

using nfd::Face;

class VmacFixture : public GlobalIoTimeFixture, public KeyChainFixture
{
protected:
  VmacFixture()
  {
    // By default, VmacLinkService is created with default options.
    // Test cases may invoke initialize() with alternate options.
    initialize({});
  }

  void
  initialize(const VmacLinkService::Options& options,
             ssize_t mtu = 2000,
             ssize_t sendQueueCapacity = QUEUE_UNSUPPORTED)
  {
    face = make_unique<Face>(make_unique<VmacLinkService>(options),
                             make_unique<VmacTransport>(mtu));
    service = static_cast<VmacLinkService*>(face->getLinkService());
    transport = static_cast<VmacTransport*>(face->getTransport());

    face->afterReceiveInterest.connect(
      [this] (const Interest& interest, const EndpointId&) { receivedInterests.push_back(interest); });
    face->afterReceiveData.connect(
      [this] (const Data& data, const EndpointId&) { receivedData.push_back(data); });
    face->afterReceiveNack.connect(
      [this] (const lp::Nack& nack, const EndpointId&) { receivedNacks.push_back(nack); });
  }

  lp::PrefixAnnouncementHeader
  makePrefixAnnHeader(const Name& announcedName)
  {
    return lp::PrefixAnnouncementHeader{signPrefixAnn(makePrefixAnn(announcedName, 1_h),
                                                      m_keyChain, ndn::signingWithSha256())};
  }

protected:
  unique_ptr<Face> face;
  VmacLinkService* service = nullptr;
  VmacTransport* transport = nullptr;
  std::vector<Interest> receivedInterests;
  std::vector<Data> receivedData;
  std::vector<lp::Nack> receivedNacks;
};

BOOST_FIXTURE_TEST_SUITE(TestVmac, VmacFixture)

BOOST_AUTO_TEST_SUITE(SimpleSendReceive) // send and receive without other fields

BOOST_AUTO_TEST_CASE(SendInterest)
{
  // Initialize with Options that disables all services
  VmacLinkService::Options options;
  options.allowLocalFields = false;
  options.reliabilityOptions.isEnabled = true;
  options.allowFragmentation = true;
  
  initialize(options);

  auto interest1 =
  makeInterest("/localhost/test/");
  BOOST_CHECK(transport != nullptr);
  BOOST_CHECK(service != nullptr);
  face->sendInterest(*interest1, 0);
  BOOST_CHECK_EQUAL(service->getCounters().nOutInterests, 1);
}

BOOST_AUTO_TEST_CASE(SendData)
{
  // Initialize with Options that disables all services
  VmacLinkService::Options options;
  options.allowLocalFields = false;
  options.reliabilityOptions.isEnabled = true;
  options.allowFragmentation = true;

  initialize(options);

  auto data1 = makeData("/localhost/test");
  face->sendData(*data1, 0);

  BOOST_CHECK_EQUAL(service->getCounters().nOutData, 1);
}

BOOST_AUTO_TEST_CASE(SendNack)
{
  // Initialize with Options that disables all services
  VmacLinkService::Options options;
  options.allowLocalFields = false;
  options.reliabilityOptions.isEnabled = true;
  options.allowFragmentation = true;

  initialize(options);

  auto nack1 = makeNack(*makeInterest("/localhost/test", false, nullopt, 323),
                        lp::NackReason::NO_ROUTE);
  face->sendNack(nack1, 0);

  BOOST_CHECK_EQUAL(service->getCounters().nOutNacks, 1);
}

BOOST_AUTO_TEST_SUITE_END() // SimpleSendReceive

BOOST_AUTO_TEST_SUITE(Fragmentation)

BOOST_AUTO_TEST_CASE(SendFragmentedInterest)
{
  // Initialize with Options that disables all services
  VmacLinkService::Options options;
  options.allowLocalFields = false;
  options.reliabilityOptions.isEnabled = true;
  options.allowFragmentation = true;
  
  initialize(options, 100);

  char interestName[101] = "/localhost/test/";
  for (int i=16; i<100; i++)
	  interestName[i] = 'a';
  interestName[100] = '\0';
  printf("%s %d\n", interestName, strlen(interestName));
  auto interest1 =
  makeInterest(interestName);
  BOOST_CHECK(transport != nullptr);
  BOOST_CHECK(service != nullptr);
  face->sendInterest(*interest1, 0);
  BOOST_CHECK_EQUAL(service->getCounters().nOutInterests, 1);
}

BOOST_AUTO_TEST_CASE(SendFragmentedData)
{
  // Initialize with Options that disables all services
  VmacLinkService::Options options;
  options.allowLocalFields = false;
  options.reliabilityOptions.isEnabled = true;
  options.allowFragmentation = true;

  initialize(options, 50);

  char data[101] = "/localhost/test/";
  for (int i=16; i<100; i++)
	  data[i] = 'a';
  data[100] = '\0';
  auto data1 = makeData(data);
  face->sendData(*data1, 0);

  BOOST_CHECK_EQUAL(service->getCounters().nOutData, 1);
}

BOOST_AUTO_TEST_SUITE_END() // Fragmentation
BOOST_AUTO_TEST_SUITE_END() // TestVmac
BOOST_AUTO_TEST_SUITE_END() // Face

} // namespace tests
} // namespace face
} // namespace nfd
