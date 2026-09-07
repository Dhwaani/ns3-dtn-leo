/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "cbor.h"

namespace ns3
{
namespace dtn
{

// --- CborWriter ------------------------------------------------------------

const std::vector<uint8_t>&
CborWriter::Bytes() const
{
    return m_buffer;
}

void
CborWriter::WriteHead(uint8_t majorType, uint64_t value)
{
    const uint8_t mt = static_cast<uint8_t>(majorType << 5);
    if (value < 24)
    {
        m_buffer.push_back(mt | static_cast<uint8_t>(value));
    }
    else if (value <= 0xFF)
    {
        m_buffer.push_back(mt | 24);
        m_buffer.push_back(static_cast<uint8_t>(value));
    }
    else if (value <= 0xFFFF)
    {
        m_buffer.push_back(mt | 25);
        m_buffer.push_back(static_cast<uint8_t>(value >> 8));
        m_buffer.push_back(static_cast<uint8_t>(value));
    }
    else if (value <= 0xFFFFFFFF)
    {
        m_buffer.push_back(mt | 26);
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            m_buffer.push_back(static_cast<uint8_t>(value >> shift));
        }
    }
    else
    {
        m_buffer.push_back(mt | 27);
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            m_buffer.push_back(static_cast<uint8_t>(value >> shift));
        }
    }
}

void
CborWriter::WriteUint(uint64_t value)
{
    WriteHead(0, value);
}

void
CborWriter::WriteByteString(const std::vector<uint8_t>& bytes)
{
    WriteHead(2, bytes.size());
    m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
}

void
CborWriter::WriteTextString(const std::string& text)
{
    WriteHead(3, text.size());
    m_buffer.insert(m_buffer.end(), text.begin(), text.end());
}

void
CborWriter::WriteArrayHeader(uint64_t count)
{
    WriteHead(4, count);
}

// --- CborReader ------------------------------------------------------------

CborReader::CborReader(const std::vector<uint8_t>& bytes)
    : m_bytes(bytes)
{
}

bool
CborReader::Ok() const
{
    return m_ok;
}

std::size_t
CborReader::Remaining() const
{
    return m_pos <= m_bytes.size() ? m_bytes.size() - m_pos : 0;
}

bool
CborReader::ReadHead(uint8_t expectedMajor, uint64_t& value)
{
    if (!m_ok || m_pos >= m_bytes.size())
    {
        m_ok = false;
        return false;
    }
    const uint8_t initial = m_bytes[m_pos++];
    const uint8_t major = initial >> 5;
    const uint8_t info = initial & 0x1F;
    if (major != expectedMajor)
    {
        m_ok = false;
        return false;
    }

    if (info < 24)
    {
        value = info;
        return true;
    }

    std::size_t nBytes = 0;
    switch (info)
    {
    case 24: nBytes = 1; break;
    case 25: nBytes = 2; break;
    case 26: nBytes = 4; break;
    case 27: nBytes = 8; break;
    default: m_ok = false; return false; // reserved / indefinite not supported
    }

    if (m_pos + nBytes > m_bytes.size())
    {
        m_ok = false;
        return false;
    }
    value = 0;
    for (std::size_t i = 0; i < nBytes; ++i)
    {
        value = (value << 8) | m_bytes[m_pos++];
    }
    return true;
}

uint64_t
CborReader::ReadUint()
{
    uint64_t v = 0;
    ReadHead(0, v);
    return v;
}

std::vector<uint8_t>
CborReader::ReadByteString()
{
    uint64_t len = 0;
    if (!ReadHead(2, len) || m_pos + len > m_bytes.size())
    {
        m_ok = false;
        return {};
    }
    std::vector<uint8_t> out(m_bytes.begin() + m_pos, m_bytes.begin() + m_pos + len);
    m_pos += len;
    return out;
}

std::string
CborReader::ReadTextString()
{
    uint64_t len = 0;
    if (!ReadHead(3, len) || m_pos + len > m_bytes.size())
    {
        m_ok = false;
        return {};
    }
    std::string out(m_bytes.begin() + m_pos, m_bytes.begin() + m_pos + len);
    m_pos += len;
    return out;
}

uint64_t
CborReader::ReadArrayHeader()
{
    uint64_t v = 0;
    ReadHead(4, v);
    return v;
}

} // namespace dtn
} // namespace ns3
