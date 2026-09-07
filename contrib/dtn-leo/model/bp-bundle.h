/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_BP_BUNDLE_H
#define DTN_LEO_BP_BUNDLE_H

#include "bp-endpoint-id.h"
#include "cbor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief BPv7 (RFC 9171) primary block fields relevant to routing/forwarding.
 *
 * We model the semantically important fields. CRC is modeled as a type code
 * only (the actual CRC bytes are not computed, since ns-3 channels already
 * inject bit errors and we validate correctness at the application layer).
 */
struct PrimaryBlock
{
    uint8_t version{7};
    uint64_t processingControlFlags{0};
    uint8_t crcType{0};
    EndpointId destination;
    EndpointId source;
    EndpointId reportTo;
    uint64_t creationTimestampTime{0}; //!< DTN time (ms since 2000-01-01), simplified to sim ms
    uint64_t creationTimestampSeq{0};
    uint64_t lifetimeMs{0}; //!< bundle lifetime in milliseconds

    // Bundle processing control flag bits we care about (subset of RFC 9171 4.2.3)
    static constexpr uint64_t FLAG_IS_ADMIN_RECORD = 0x02;
    static constexpr uint64_t FLAG_DO_NOT_FRAGMENT = 0x04;

    void Encode(CborWriter& w) const;
    static PrimaryBlock Decode(CborReader& r);
};

/**
 * \ingroup dtn-leo
 * \brief BPv7 canonical block. Block type 1 is the payload block.
 */
struct CanonicalBlock
{
    uint64_t blockTypeCode{1}; //!< 1 == payload
    uint64_t blockNumber{1};
    uint64_t blockControlFlags{0};
    uint8_t crcType{0};
    std::vector<uint8_t> data; //!< block-type-specific data (payload bytes for type 1)

    void Encode(CborWriter& w) const;
    static CanonicalBlock Decode(CborReader& r);
};

/**
 * \ingroup dtn-leo
 * \brief A complete BPv7 bundle: one primary block plus one or more canonical
 *        blocks. Encoded on the wire as an indefinite... (here definite) CBOR
 *        array of blocks, per RFC 9171 section 4.
 *
 * In addition to the on-wire fields, we attach lightweight *local* metadata
 * (not serialized) used by the store-carry-forward scheduler and CGR router:
 * priority class and the simulation time at which the bundle entered the store.
 */
class Bundle
{
  public:
    enum Priority
    {
        BULK = 0,
        NORMAL = 1,
        EXPEDITED = 2
    };

    PrimaryBlock primary;
    std::vector<CanonicalBlock> canonicalBlocks;

    // Local-only metadata (never serialized):
    Priority priority{NORMAL};
    double enqueuedAtSec{0.0};
    uint64_t sizeBytesHint{0};

    /** Convenience: set/get the payload block (block type 1). */
    void SetPayload(const std::vector<uint8_t>& payload);
    std::vector<uint8_t> GetPayload() const;

    /** Total serialized size in bytes (used for contact-volume booking). */
    uint64_t GetSerializedSizeBytes() const;

    /** Encode the whole bundle to CBOR (array of blocks). */
    std::vector<uint8_t> Encode() const;
    /** Decode a whole bundle from CBOR produced by Encode(). */
    static Bundle Decode(const std::vector<uint8_t>& bytes, bool& ok);

    /** Unique-ish identifier: (source-node, creation-time, creation-seq). */
    std::string GetId() const;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_BP_BUNDLE_H */
