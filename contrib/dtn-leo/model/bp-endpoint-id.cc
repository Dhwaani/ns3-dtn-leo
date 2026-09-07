/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "bp-endpoint-id.h"

namespace ns3
{
namespace dtn
{

EndpointId::EndpointId(uint64_t node, uint64_t service)
    : m_node(node),
      m_service(service)
{
}

uint64_t
EndpointId::GetNode() const
{
    return m_node;
}

uint64_t
EndpointId::GetService() const
{
    return m_service;
}

bool
EndpointId::IsNull() const
{
    return m_node == 0 && m_service == 0;
}

void
EndpointId::Encode(CborWriter& w) const
{
    // RFC 9171 EID: [ uri-scheme-code, SSP ]. For ipn, SSP is [ node, service ].
    w.WriteArrayHeader(2);
    w.WriteUint(IPN_SCHEME_CODE);
    w.WriteArrayHeader(2);
    w.WriteUint(m_node);
    w.WriteUint(m_service);
}

EndpointId
EndpointId::Decode(CborReader& r)
{
    r.ReadArrayHeader();          // outer [scheme, ssp]
    r.ReadUint();                 // scheme code (assumed ipn == 2)
    r.ReadArrayHeader();          // ssp [node, service]
    const uint64_t node = r.ReadUint();
    const uint64_t service = r.ReadUint();
    return EndpointId(node, service);
}

std::string
EndpointId::ToString() const
{
    return "ipn:" + std::to_string(m_node) + "." + std::to_string(m_service);
}

bool
EndpointId::operator==(const EndpointId& o) const
{
    return m_node == o.m_node && m_service == o.m_service;
}

bool
EndpointId::operator<(const EndpointId& o) const
{
    if (m_node != o.m_node)
    {
        return m_node < o.m_node;
    }
    return m_service < o.m_service;
}

} // namespace dtn
} // namespace ns3
