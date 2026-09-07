/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "bp-bundle.h"

namespace ns3
{
namespace dtn
{

// --- PrimaryBlock ----------------------------------------------------------

void
PrimaryBlock::Encode(CborWriter& w) const
{
    // RFC 9171 4.3.1: primary block is a CBOR array. With crcType==0 there are
    // 8 elements (no CRC field). We always emit crcType==0 here.
    w.WriteArrayHeader(8);
    w.WriteUint(version);
    w.WriteUint(processingControlFlags);
    w.WriteUint(crcType);
    destination.Encode(w);
    source.Encode(w);
    reportTo.Encode(w);
    // creation timestamp is itself a 2-array [time, seq]
    w.WriteArrayHeader(2);
    w.WriteUint(creationTimestampTime);
    w.WriteUint(creationTimestampSeq);
    w.WriteUint(lifetimeMs);
}

PrimaryBlock
PrimaryBlock::Decode(CborReader& r)
{
    PrimaryBlock pb;
    r.ReadArrayHeader(); // 8 elements
    pb.version = static_cast<uint8_t>(r.ReadUint());
    pb.processingControlFlags = r.ReadUint();
    pb.crcType = static_cast<uint8_t>(r.ReadUint());
    pb.destination = EndpointId::Decode(r);
    pb.source = EndpointId::Decode(r);
    pb.reportTo = EndpointId::Decode(r);
    r.ReadArrayHeader(); // creation timestamp [time, seq]
    pb.creationTimestampTime = r.ReadUint();
    pb.creationTimestampSeq = r.ReadUint();
    pb.lifetimeMs = r.ReadUint();
    return pb;
}

// --- CanonicalBlock --------------------------------------------------------

void
CanonicalBlock::Encode(CborWriter& w) const
{
    // RFC 9171 4.3.2: [ type, number, flags, crcType, data(bstr) ] (5 elems, no CRC).
    w.WriteArrayHeader(5);
    w.WriteUint(blockTypeCode);
    w.WriteUint(blockNumber);
    w.WriteUint(blockControlFlags);
    w.WriteUint(crcType);
    w.WriteByteString(data);
}

CanonicalBlock
CanonicalBlock::Decode(CborReader& r)
{
    CanonicalBlock cb;
    r.ReadArrayHeader(); // 5 elements
    cb.blockTypeCode = r.ReadUint();
    cb.blockNumber = r.ReadUint();
    cb.blockControlFlags = r.ReadUint();
    cb.crcType = static_cast<uint8_t>(r.ReadUint());
    cb.data = r.ReadByteString();
    return cb;
}

// --- Bundle ----------------------------------------------------------------

void
Bundle::SetPayload(const std::vector<uint8_t>& payload)
{
    for (auto& b : canonicalBlocks)
    {
        if (b.blockTypeCode == 1)
        {
            b.data = payload;
            return;
        }
    }
    CanonicalBlock cb;
    cb.blockTypeCode = 1;
    cb.blockNumber = 1;
    cb.data = payload;
    canonicalBlocks.push_back(cb);
}

std::vector<uint8_t>
Bundle::GetPayload() const
{
    for (const auto& b : canonicalBlocks)
    {
        if (b.blockTypeCode == 1)
        {
            return b.data;
        }
    }
    return {};
}

std::vector<uint8_t>
Bundle::Encode() const
{
    CborWriter w;
    // A bundle is a CBOR array of its blocks (primary first).
    w.WriteArrayHeader(1 + canonicalBlocks.size());
    primary.Encode(w);
    for (const auto& cb : canonicalBlocks)
    {
        cb.Encode(w);
    }
    return w.Bytes();
}

Bundle
Bundle::Decode(const std::vector<uint8_t>& bytes, bool& ok)
{
    Bundle b;
    CborReader r(bytes);
    const uint64_t nBlocks = r.ReadArrayHeader();
    if (!r.Ok() || nBlocks == 0)
    {
        ok = false;
        return b;
    }
    b.primary = PrimaryBlock::Decode(r);
    for (uint64_t i = 1; i < nBlocks; ++i)
    {
        b.canonicalBlocks.push_back(CanonicalBlock::Decode(r));
    }
    ok = r.Ok();
    return b;
}

uint64_t
Bundle::GetSerializedSizeBytes() const
{
    return Encode().size();
}

std::string
Bundle::GetId() const
{
    return std::to_string(primary.source.GetNode()) + ":" +
           std::to_string(primary.creationTimestampTime) + ":" +
           std::to_string(primary.creationTimestampSeq);
}

} // namespace dtn
} // namespace ns3
