/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "cgr-router.h"

#include "ns3/log.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnCgrRouter");

CgrRouter::CgrRouter(ContactPlan* plan)
    : m_plan(plan)
{
}

CgrRoute
CgrRouter::ComputeRoute(uint32_t source,
                        uint32_t dest,
                        double bundleBytes,
                        double currentTimeSec,
                        double expiryTimeSec,
                        uint32_t excludeFirstHop) const
{
    CgrRoute result;
    if (!m_plan)
    {
        return result;
    }

    const auto& contacts = m_plan->GetContacts();

    // Dijkstra over node arrival-times. label[n] = earliest time bundle present at n.
    std::map<uint32_t, double> arrival;
    std::map<uint32_t, std::size_t> viaContact; // node -> contact index used to reach it
    std::map<uint32_t, uint32_t> predecessor;   // node -> previous node

    arrival[source] = currentTimeSec;

    // Priority queue keyed by arrival time (min-heap).
    using PqItem = std::pair<double, uint32_t>;
    std::priority_queue<PqItem, std::vector<PqItem>, std::greater<PqItem>> pq;
    pq.push({currentTimeSec, source});

    std::set<uint32_t> settled;

    while (!pq.empty())
    {
        auto [t, u] = pq.top();
        pq.pop();
        if (settled.count(u))
        {
            continue;
        }
        settled.insert(u);
        if (u == dest)
        {
            break;
        }

        // Relax every contact departing from u.
        for (std::size_t ci = 0; ci < contacts.size(); ++ci)
        {
            const Contact& c = contacts[ci];
            if (c.fromNode != u)
            {
                continue;
            }
            // Enforce return-to-sender / excluded first hop only on the first hop.
            if (u == source && excludeFirstHop != 0 && c.toNode == excludeFirstHop)
            {
                continue;
            }
            if (c.dataRateBps <= 0.0)
            {
                continue;
            }
            const double txTime = bundleBytes / (c.dataRateBps / 8.0);
            // Earliest we can start sending on this contact.
            const double departure = std::max(t, c.startSec);
            // The whole bundle must complete before the contact ends.
            if (departure + txTime > c.endSec)
            {
                continue;
            }
            // Respect residual volume.
            if (c.ResidualBytes() < bundleBytes)
            {
                continue;
            }
            const double arriveV = departure + txTime + c.owltSec;
            if (arriveV > expiryTimeSec)
            {
                continue; // would arrive after the bundle is dead
            }
            auto it = arrival.find(c.toNode);
            if (it == arrival.end() || arriveV < it->second)
            {
                arrival[c.toNode] = arriveV;
                viaContact[c.toNode] = ci;
                predecessor[c.toNode] = u;
                pq.push({arriveV, c.toNode});
            }
        }
    }

    if (!arrival.count(dest))
    {
        NS_LOG_LOGIC("CGR: no route " << source << "->" << dest);
        return result; // not found
    }

    // Reconstruct the hop sequence and contact list from dest back to source.
    result.found = true;
    result.bestDeliveryTimeSec = arrival[dest];

    std::vector<uint32_t> revNodes;
    std::vector<std::size_t> revContacts;
    uint32_t cur = dest;
    while (cur != source)
    {
        revNodes.push_back(cur);
        revContacts.push_back(viaContact[cur]);
        cur = predecessor[cur];
    }
    revNodes.push_back(source);

    result.hopSequence.assign(revNodes.rbegin(), revNodes.rend());
    result.contactIndices.assign(revContacts.rbegin(), revContacts.rend());
    result.nextHopNode = result.hopSequence.size() > 1 ? result.hopSequence[1] : dest;

    NS_LOG_LOGIC("CGR: route " << source << "->" << dest << " nextHop=" << result.nextHopNode
                               << " deliverAt=" << result.bestDeliveryTimeSec << "s hops="
                               << result.hopSequence.size());
    return result;
}

void
CgrRouter::BookRoute(const CgrRoute& route, double bundleBytes)
{
    if (!m_plan || !route.found)
    {
        return;
    }
    auto& contacts = m_plan->GetContactsMutable();
    for (std::size_t idx : route.contactIndices)
    {
        if (idx < contacts.size())
        {
            contacts[idx].bookedBytes += bundleBytes;
        }
    }
}

} // namespace dtn
} // namespace ns3
