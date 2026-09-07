/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "bp-header.h"

namespace ns3
{
namespace dtn
{

BpHeader::BpHeader(const Bundle& bundle)
    : m_bundle(bundle)
{
}

const Bundle&
BpHeader::GetBundle() const
{
    return m_bundle;
}

void
BpHeader::SetBundle(const Bundle& bundle)
{
    m_bundle = bundle;
    m_cache.clear();
}

TypeId
BpHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::dtn::BpHeader")
                            .SetParent<Header>()
                            .SetGroupName("DtnLeo")
                            .AddConstructor<BpHeader>();
    return tid;
}

TypeId
BpHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BpHeader::GetSerializedSize() const
{
    if (m_cache.empty())
    {
        m_cache = m_bundle.Encode();
    }
    return 4 + static_cast<uint32_t>(m_cache.size());
}

void
BpHeader::Serialize(Buffer::Iterator start) const
{
    if (m_cache.empty())
    {
        m_cache = m_bundle.Encode();
    }
    start.WriteHtonU32(static_cast<uint32_t>(m_cache.size()));
    for (uint8_t byte : m_cache)
    {
        start.WriteU8(byte);
    }
}

uint32_t
BpHeader::Deserialize(Buffer::Iterator start)
{
    const uint32_t len = start.ReadNtohU32();
    std::vector<uint8_t> blob(len);
    for (uint32_t i = 0; i < len; ++i)
    {
        blob[i] = start.ReadU8();
    }
    bool ok = false;
    m_bundle = Bundle::Decode(blob, ok);
    m_cache = blob;
    return 4 + len;
}

void
BpHeader::Print(std::ostream& os) const
{
    os << "BPv7 bundle id=" << m_bundle.GetId()
       << " src=" << m_bundle.primary.source.ToString()
       << " dst=" << m_bundle.primary.destination.ToString()
       << " payloadBytes=" << m_bundle.GetPayload().size();
}

} // namespace dtn
} // namespace ns3
