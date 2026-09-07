/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Minimal CBOR (RFC 8949) codec, sufficient for encoding/decoding the
 * subset of CBOR used by Bundle Protocol version 7 (RFC 9171):
 *   - unsigned integers      (major type 0)
 *   - byte strings           (major type 2)
 *   - text strings           (major type 3)
 *   - definite-length arrays (major type 4)
 *
 * This is intentionally small and dependency-free so that the DTN model can
 * be dropped into any ns-3 tree without pulling in an external CBOR library.
 * It is NOT a general-purpose CBOR implementation (no maps, tags, floats,
 * indefinite-length items, or negative integers).
 */
#ifndef DTN_LEO_CBOR_H
#define DTN_LEO_CBOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Append-only CBOR writer over a std::vector<uint8_t>.
 */
class CborWriter
{
  public:
    /** \return the encoded bytes accumulated so far. */
    const std::vector<uint8_t>& Bytes() const;

    /** Encode an unsigned integer (CBOR major type 0). */
    void WriteUint(uint64_t value);
    /** Encode a byte string (CBOR major type 2). */
    void WriteByteString(const std::vector<uint8_t>& bytes);
    /** Encode a text string (CBOR major type 3). */
    void WriteTextString(const std::string& text);
    /** Encode the head of a definite-length array of \p count items (major type 4). */
    void WriteArrayHeader(uint64_t count);

  private:
    void WriteHead(uint8_t majorType, uint64_t value);
    std::vector<uint8_t> m_buffer;
};

/**
 * \ingroup dtn-leo
 * \brief Sequential CBOR reader over a byte span.
 *
 * All Read* methods advance an internal cursor. On malformed/truncated input
 * they set an error flag (see Ok()) and return zero/empty values.
 */
class CborReader
{
  public:
    explicit CborReader(const std::vector<uint8_t>& bytes);

    uint64_t ReadUint();
    std::vector<uint8_t> ReadByteString();
    std::string ReadTextString();
    /** \return the declared element count of the next array. */
    uint64_t ReadArrayHeader();

    /** \return false once any decode error has occurred. */
    bool Ok() const;
    /** \return number of bytes not yet consumed. */
    std::size_t Remaining() const;

  private:
    bool ReadHead(uint8_t expectedMajor, uint64_t& value);
    const std::vector<uint8_t>& m_bytes;
    std::size_t m_pos{0};
    bool m_ok{true};
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_CBOR_H */
