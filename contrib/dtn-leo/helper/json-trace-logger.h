/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_JSON_TRACE_LOGGER_H
#define DTN_LEO_JSON_TRACE_LOGGER_H

#include "ns3/bundle-protocol-agent.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Records BundleProtocolAgent trace events (and the node set) to a JSON
 *        file that the dtn-leo-globe visualizer replays. See viz/README.md for
 *        the schema this produces.
 *
 * Header-only, and deliberately tiny. It is NOT an ns3::Object; keep one
 * instance alive on the stack for the whole run. It taps the agent's enriched
 * traced sources:
 *   Tx      (bundleId, fromNode, toNode, priority)  -> a "tx" event
 *   Deliver (bundleId, atNode)                       -> a "deliver" event
 *   Defer   (bundleId, atNode, priority)             -> a "defer" event
 * A run of per-tick defers for the same bundle is collapsed to one event.
 *
 * Bundle ids from the agent are strings ("src-time-seq"); they are mapped to
 * small integers so the JSON stays compact and matches the globe's schema.
 *
 * \code
 *   JsonTraceLogger logger;
 *   logger.SetLabel("6x8 constellation");
 *   // one row per node (orbit params you already pass to the propagator):
 *   logger.AddSatellite(0, 780, 70, 0, 0);          // id, altKm, incl, raan, phase (deg)
 *   logger.AddGround(48, 37.4, -122.0, "src");       // id, lat, lon, role
 *   logger.AddGround(49,  1.3,  103.8, "dst");
 *   for (uint32_t i = 0; i < n; ++i) logger.Connect(dtn.GetAgent(i));
 *   Simulator::Run();
 *   logger.Write("dtn-leo-events.json");
 * \endcode
 */
class JsonTraceLogger
{
  public:
    void SetLabel(const std::string& label) { m_label = label; }

    /** Register a satellite by its circular-orbit parameters (degrees). */
    void AddSatellite(uint32_t id,
                      double altKm,
                      double inclDeg,
                      double raanDeg,
                      double phaseDeg,
                      double soc = 0.8)
    {
        std::ostringstream os;
        os << "{ \"id\": " << id << ", \"type\": \"sat\", \"orbit\": { \"altKm\": " << altKm
           << ", \"inclDeg\": " << inclDeg << ", \"raanDeg\": " << raanDeg
           << ", \"phaseDeg\": " << phaseDeg << " }, \"soc\": " << soc << " }";
        m_nodes.push_back(os.str());
    }

    /** Register a ground station by latitude/longitude (degrees). role: "src"/"dst"/"". */
    void AddGround(uint32_t id, double latDeg, double lonDeg, const std::string& role = "")
    {
        std::ostringstream os;
        os << "{ \"id\": " << id << ", \"type\": \"ground\", \"lat\": " << latDeg
           << ", \"lon\": " << lonDeg;
        if (!role.empty())
        {
            os << ", \"role\": \"" << role << "\"";
        }
        os << " }";
        m_nodes.push_back(os.str());
    }

    /** Hook Tx / Deliver / Defer on one agent. Call for every agent. */
    void Connect(Ptr<BundleProtocolAgent> agent)
    {
        agent->TraceConnectWithoutContext("Tx", MakeCallback(&JsonTraceLogger::OnTx, this));
        agent->TraceConnectWithoutContext("Deliver",
                                          MakeCallback(&JsonTraceLogger::OnDeliver, this));
        agent->TraceConnectWithoutContext("Defer", MakeCallback(&JsonTraceLogger::OnDefer, this));
    }

    /** Write the collected scenario + events to \p path as JSON. */
    void Write(const std::string& path) const
    {
        std::ofstream f(path);
        f << "{\n";
        f << "  \"meta\": { \"label\": \"" << m_label
          << "\", \"durationSec\": " << Simulator::Now().GetSeconds() << " },\n";
        f << "  \"nodes\": [\n";
        for (std::size_t i = 0; i < m_nodes.size(); ++i)
        {
            f << "    " << m_nodes[i] << (i + 1 < m_nodes.size() ? ",\n" : "\n");
        }
        f << "  ],\n";
        f << "  \"events\": [\n";
        for (std::size_t i = 0; i < m_events.size(); ++i)
        {
            f << "    " << m_events[i] << (i + 1 < m_events.size() ? ",\n" : "\n");
        }
        f << "  ]\n";
        f << "}\n";
    }

  private:
    static const char* PrioName(uint8_t p)
    {
        switch (p)
        {
        case 0:
            return "BULK";
        case 2:
            return "EXPEDITED";
        default:
            return "NORMAL";
        }
    }

    int MapId(const std::string& bid)
    {
        auto it = m_bundleIds.find(bid);
        if (it != m_bundleIds.end())
        {
            return it->second;
        }
        int n = static_cast<int>(m_bundleIds.size()) + 1;
        m_bundleIds.emplace(bid, n);
        return n;
    }

    static double Now() { return Simulator::Now().GetSeconds(); }

    void OnTx(std::string id, uint32_t from, uint32_t to, uint8_t prio)
    {
        m_lastWasDefer[id] = false;
        std::ostringstream os;
        os << "{ \"tSec\": " << Now() << ", \"type\": \"tx\", \"bundleId\": " << MapId(id)
           << ", \"from\": " << from << ", \"to\": " << to << ", \"priority\": \""
           << PrioName(prio) << "\" }";
        m_events.push_back(os.str());
    }

    void OnDeliver(std::string id, uint32_t at)
    {
        m_lastWasDefer[id] = false;
        std::ostringstream os;
        os << "{ \"tSec\": " << Now() << ", \"type\": \"deliver\", \"bundleId\": " << MapId(id)
           << ", \"from\": " << at << ", \"to\": " << at << " }";
        m_events.push_back(os.str());
    }

    void OnDefer(std::string id, uint32_t at, uint8_t prio)
    {
        // Collapse a run of per-tick defers for the same bundle into one event.
        if (m_lastWasDefer[id])
        {
            return;
        }
        m_lastWasDefer[id] = true;
        std::ostringstream os;
        os << "{ \"tSec\": " << Now() << ", \"type\": \"defer\", \"bundleId\": " << MapId(id)
           << ", \"from\": " << at << ", \"to\": " << at << ", \"priority\": \"" << PrioName(prio)
           << "\" }";
        m_events.push_back(os.str());
    }

    std::string m_label{"ns-3 run"};
    std::vector<std::string> m_nodes;
    std::vector<std::string> m_events;
    std::unordered_map<std::string, int> m_bundleIds;
    std::unordered_map<std::string, bool> m_lastWasDefer;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_JSON_TRACE_LOGGER_H */
