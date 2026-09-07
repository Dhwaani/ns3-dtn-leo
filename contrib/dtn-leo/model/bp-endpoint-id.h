/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_BP_ENDPOINT_ID_H
#define DTN_LEO_BP_ENDPOINT_ID_H

#include "cbor.h"

#include <cstdint>
#include <string>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief A Bundle Protocol v7 endpoint identifier restricted to the "ipn"
 *        scheme (RFC 9171 / RFC 6260): ipn:NODE.SERVICE.
 *
 * The ipn scheme is used (rather than the "dtn" URI scheme) because it maps
 * cleanly onto integer node identifiers, keeps bundles compact, and is the
 * scheme used by NASA/JPL ION, which we validate contact plans against.
 */
class EndpointId
{
  public:
    EndpointId() = default;
    EndpointId(uint64_t node, uint64_t service);

    uint64_t GetNode() const;
    uint64_t GetService() const;

    /** \return true for the null endpoint ipn:0.0. */
    bool IsNull() const;

    /** Encode as CBOR: [uriSchemeCode(2), [node, service]]. */
    void Encode(CborWriter& w) const;
    /** Decode from CBOR produced by Encode(). */
    static EndpointId Decode(CborReader& r);

    std::string ToString() const;
    bool operator==(const EndpointId& o) const;
    bool operator<(const EndpointId& o) const;

  private:
    static constexpr uint64_t IPN_SCHEME_CODE = 2;
    uint64_t m_node{0};
    uint64_t m_service{0};
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_BP_ENDPOINT_ID_H */
