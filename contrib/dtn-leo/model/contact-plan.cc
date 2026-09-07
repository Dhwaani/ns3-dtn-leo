/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "contact-plan.h"

#include "ns3/log.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnContactPlan");

double
Contact::DurationSec() const
{
    return std::max(0.0, endSec - startSec);
}

double
Contact::CapacityBytes() const
{
    return (dataRateBps / 8.0) * DurationSec();
}

double
Contact::ResidualBytes() const
{
    return std::max(0.0, CapacityBytes() - bookedBytes);
}

void
ContactPlan::AddContact(const Contact& c)
{
    m_contacts.push_back(c);
}

const std::vector<Contact>&
ContactPlan::GetContacts() const
{
    return m_contacts;
}

std::vector<Contact>&
ContactPlan::GetContactsMutable()
{
    return m_contacts;
}

std::size_t
ContactPlan::Size() const
{
    return m_contacts.size();
}

void
ContactPlan::Clear()
{
    m_contacts.clear();
}

void
ContactPlan::ResetBookings()
{
    for (auto& c : m_contacts)
    {
        c.bookedBytes = 0.0;
    }
}

std::size_t
ContactPlan::LoadFromIonFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        NS_LOG_WARN("Could not open ION contact plan: " << path);
        return 0;
    }

    // Temporary owlt table keyed by (from,to,start) so 'range' lines can be
    // matched to their 'contact' lines regardless of ordering.
    std::vector<Contact> contacts;
    struct RangeRec
    {
        uint32_t from, to;
        double start, end, owlt;
    };
    std::vector<RangeRec> ranges;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream ss(line);
        std::string a, kind, sStart, sEnd;
        if (!(ss >> a >> kind))
        {
            continue;
        }
        if (a != "a")
        {
            continue;
        }
        uint32_t from = 0, to = 0;
        double start = 0, end = 0, val = 0;
        if (!(ss >> sStart >> sEnd >> from >> to >> val))
        {
            continue;
        }
        // strip leading '+' used by ION for relative times
        auto parseTime = [](std::string t) {
            if (!t.empty() && t[0] == '+')
            {
                t = t.substr(1);
            }
            return std::stod(t);
        };
        start = parseTime(sStart);
        end = parseTime(sEnd);

        if (kind == "contact")
        {
            Contact c;
            c.fromNode = from;
            c.toNode = to;
            c.startSec = start;
            c.endSec = end;
            c.dataRateBps = val * 8.0; // ION rate is bytes/sec
            contacts.push_back(c);
        }
        else if (kind == "range")
        {
            ranges.push_back({from, to, start, end, val});
        }
    }

    // Attach owlt from matching range records.
    for (auto& c : contacts)
    {
        for (const auto& rr : ranges)
        {
            if (rr.from == c.fromNode && rr.to == c.toNode && rr.start <= c.startSec &&
                rr.end >= c.endSec)
            {
                c.owltSec = rr.owlt;
                break;
            }
        }
        m_contacts.push_back(c);
    }
    NS_LOG_INFO("Loaded " << contacts.size() << " contacts from " << path);
    return contacts.size();
}

void
ContactPlan::SaveToIonFile(const std::string& path) const
{
    std::ofstream out(path);
    if (!out.is_open())
    {
        NS_LOG_WARN("Could not write ION contact plan: " << path);
        return;
    }
    out << "# dtn-leo generated contact plan (ION subset)\n";
    for (const auto& c : m_contacts)
    {
        out << "a contact +" << c.startSec << " +" << c.endSec << " " << c.fromNode << " "
            << c.toNode << " " << (c.dataRateBps / 8.0) << "\n";
        out << "a range +" << c.startSec << " +" << c.endSec << " " << c.fromNode << " " << c.toNode
            << " " << c.owltSec << "\n";
    }
}

} // namespace dtn
} // namespace ns3
