/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_BP_HEADER_H
#define DTN_LEO_BP_HEADER_H

#include "bp-bundle.h"

#include "ns3/header.h"

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief ns-3 Header adaptor that serializes a BPv7 Bundle (CBOR) into a
 *        Packet buffer. This lets bundles ride any ns-3 NetDevice/Channel,
 *        including the LEO MockChannel or a point-to-point ISL link.
 *
 * On the wire we prepend a 4-byte length prefix followed by the CBOR blob so
 * that a convergence layer reading a byte stream (e.g. a TCP-like CLA) can
 * frame bundles unambiguously.
 */
class BpHeader : public Header
{
  public:
    BpHeader() = default;
    explicit BpHeader(const Bundle& bundle);

    const Bundle& GetBundle() const;
    void SetBundle(const Bundle& bundle);

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    Bundle m_bundle;
    mutable std::vector<uint8_t> m_cache; //!< memoized CBOR encoding
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_BP_HEADER_H */
