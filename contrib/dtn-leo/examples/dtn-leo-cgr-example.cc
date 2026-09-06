/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * dtn-leo-cgr-example
 * -------------------
 * A self-contained demonstration of BPv7 + Contact Graph Routing + eclipse-aware
 * store-carry-forward over a small LEO constellation.
 *
 * To keep the example runnable WITHOUT the ns-3 `leo` contrib module, node
 * positions come from a simple analytic circular-orbit oracle. The SAME oracle
 * feeds (a) the contact-plan generator and (b) a runtime "orbit driver" that
 * updates each node's ConstantPositionMobilityModel every step so the eclipse
 * power model sees moving satellites.
 *
 * TO USE THE REAL ns-3.48 LEO MOBILITY:
 *   - install the `leo` module, build the constellation with LeoOrbitNodeHelper,
 *   - replace `OrbitOracle` below with a lambda that queries the LEO mobility
 *     model's position at time t (its propagator is deterministic), and
 *   - drop the runtime "orbit driver" (the LEO mobility updates positions itself).
 *
 * Traffic: two ground stations on opposite sides of Earth exchange bundles,
 * forcing multi-hop store-carry-forward through satellites and across eclipse.
 *
 * NOTE: This file is written to compile against ns-3.48 but has NOT been built
 * in this environment. Build it inside your ns-3 tree (see docs/BUILD.md).
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/dtn-leo-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <cmath>
#include <vector>

using namespace ns3;
using namespace ns3::dtn;

NS_LOG_COMPONENT_DEFINE("DtnLeoCgrExample");

namespace
{

constexpr double kEarthR = 6.371e6;      // m
constexpr double kMu = 3.986004418e14;   // m^3/s^2 (Earth GM)

struct OrbitParams
{
    bool isGround{false};
    double radius{0.0};   // orbit radius (m) for sats
    double omega{0.0};    // angular rate (rad/s)
    double phase{0.0};    // initial phase (rad)
    double raan{0.0};     // right ascension of ascending node (rotates the plane about z)
    double incl{0.0};     // inclination (rad)
    Vector fixed{0, 0, 0}; // fixed position for ground stations
};

std::vector<OrbitParams> g_orbits;

// Analytic position oracle used by BOTH the generator and the runtime driver.
Vector
OrbitOracle(uint32_t nodeId, double t)
{
    const OrbitParams& op = g_orbits[nodeId];
    if (op.isGround)
    {
        return op.fixed;
    }
    const double ang = op.phase + op.omega * t;
    // Position in orbital plane (before inclination / RAAN rotation).
    double x = op.radius * std::cos(ang);
    double y = op.radius * std::sin(ang);
    double z = 0.0;
    // Apply inclination (rotate about x-axis).
    double y1 = y * std::cos(op.incl) - z * std::sin(op.incl);
    double z1 = y * std::sin(op.incl) + z * std::cos(op.incl);
    y = y1;
    z = z1;
    // Apply RAAN (rotate about z-axis).
    double x2 = x * std::cos(op.raan) - y * std::sin(op.raan);
    double y2 = x * std::sin(op.raan) + y * std::cos(op.raan);
    return Vector(x2, y2, z);
}

// Runtime driver: push analytic positions into ConstantPositionMobilityModel.
void
DriveOrbits(NodeContainer nodes, double step, double stop)
{
    const double now = Simulator::Now().GetSeconds();
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<MobilityModel> m = nodes.Get(i)->GetObject<MobilityModel>();
        if (m)
        {
            m->SetPosition(OrbitOracle(nodes.Get(i)->GetId(), now));
        }
    }
    if (now + step <= stop)
    {
        Simulator::Schedule(Seconds(step), &DriveOrbits, nodes, step, stop);
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t nPlanes = 3;
    uint32_t nSatsPerPlane = 4;
    double altitudeKm = 550.0;
    double simTime = 6000.0; // ~ one orbit is ~5760s at 550km
    double genStep = 10.0;
    double driveStep = 5.0;
    uint32_t nBundles = 40;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("planes", "Number of orbital planes", nPlanes);
    cmd.AddValue("satsPerPlane", "Satellites per plane", nSatsPerPlane);
    cmd.AddValue("altitudeKm", "Satellite altitude (km)", altitudeKm);
    cmd.AddValue("simTime", "Simulation duration (s)", simTime);
    cmd.AddValue("bundles", "Number of bundles to originate", nBundles);
    cmd.AddValue("verbose", "Enable component logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("DtnBundleProtocolAgent", LOG_LEVEL_INFO);
        LogComponentEnable("DtnLeoHelper", LOG_LEVEL_INFO);
        LogComponentEnable("DtnContactPlanGenerator", LOG_LEVEL_INFO);
    }

    const uint32_t nSats = nPlanes * nSatsPerPlane;
    const uint32_t nGround = 2;
    const uint32_t nNodes = nSats + nGround;

    NodeContainer nodes;
    nodes.Create(nNodes);

    // --- Build orbital parameters (node id order == creation order) ---------
    g_orbits.resize(nNodes);
    const double R = kEarthR + altitudeKm * 1e3;
    const double omega = std::sqrt(kMu / (R * R * R));
    uint32_t id = 0;
    for (uint32_t p = 0; p < nPlanes; ++p)
    {
        for (uint32_t s = 0; s < nSatsPerPlane; ++s, ++id)
        {
            OrbitParams op;
            op.isGround = false;
            op.radius = R;
            op.omega = omega;
            op.phase = 2 * M_PI * s / nSatsPerPlane;
            op.raan = M_PI * p / nPlanes;   // spread planes
            op.incl = 53.0 * M_PI / 180.0;  // Starlink-like inclination
            g_orbits[id] = op;
        }
    }
    // Two ground stations on opposite sides of Earth (fixed in inertial frame).
    {
        OrbitParams g1;
        g1.isGround = true;
        g1.fixed = Vector(kEarthR, 0, 0);
        g_orbits[nSats + 0] = g1;
        OrbitParams g2;
        g2.isGround = true;
        g2.fixed = Vector(-kEarthR, 0, 0);
        g_orbits[nSats + 1] = g2;
    }

    // --- Mobility (ConstantPosition, driven analytically at runtime) --------
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        nodes.Get(i)->GetObject<MobilityModel>()->SetPosition(OrbitOracle(i, 0.0));
    }

    // --- IP fabric for the UDP convergence layer ----------------------------
    // Simplification: a single CSMA segment provides IP reachability; the DTN
    // *contact plan* (not L2) gates when forwarding is actually allowed. See
    // docs/DESIGN.md for binding contacts to real LEO links instead.
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    std::vector<Ipv4Address> addrs;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        addrs.push_back(ifaces.GetAddress(i));
    }

    // --- Generate the contact plan from LEO geometry ------------------------
    std::vector<uint32_t> nodeIds;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        nodeIds.push_back(nodes.Get(i)->GetId());
    }
    ContactPlanGenerator::Config cfg;
    cfg.durationSec = simTime;
    cfg.stepSec = genStep;
    cfg.earthRadiusM = kEarthR;
    cfg.defaultRateBps = 10e6;
    cfg.maxRangeM = 3.0e6; // ~ISL/ground reach
    ContactPlanGenerator gen(OrbitOracle, cfg);
    ContactPlan plan = gen.Generate(nodeIds);
    std::cout << "Contact plan: " << plan.Size() << " directed contacts\n";
    plan.SaveToIonFile("dtn-leo-contact-plan.ion"); // for cross-validation vs ION

    // --- Install the DTN stack ----------------------------------------------
    DtnLeoHelper dtn;
    dtn.SetContactPlan(plan);
    ApplicationContainer apps = dtn.Install(nodes, addrs);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simTime));

    // --- Metrics ------------------------------------------------------------
    uint32_t delivered = 0;
    uint32_t deferred = 0;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<BundleProtocolAgent> a = dtn.GetAgent(i);
        a->TraceConnectWithoutContext(
            "Deliver",
            MakeBoundCallback(
                +[](uint32_t* counter, std::string, uint32_t) { (*counter)++; },
                &delivered));
        a->TraceConnectWithoutContext(
            "Defer",
            MakeBoundCallback(
                +[](uint32_t* counter, std::string, uint32_t, uint8_t) { (*counter)++; },
                &deferred));
    }

    // --- Replay logging for the dtn-leo-globe visualizer --------------------
    // Records the node set + every Tx/Deliver/Defer to a JSON file that
    // viz/dtn-leo-globe.html can load ("Load sim data"). Must be connected
    // before Simulator::Run().
    JsonTraceLogger logger;
    logger.SetLabel(std::to_string(nPlanes) + "x" + std::to_string(nSatsPerPlane) +
                    " constellation @ " + std::to_string(static_cast<int>(altitudeKm)) + " km");
    {
        uint32_t jid = 0;
        for (uint32_t p = 0; p < nPlanes; ++p)
        {
            for (uint32_t s = 0; s < nSatsPerPlane; ++s, ++jid)
            {
                logger.AddSatellite(jid,
                                    altitudeKm,
                                    53.0,                          // inclination (deg)
                                    180.0 * p / nPlanes,           // raan (deg)
                                    360.0 * s / nSatsPerPlane);    // phase (deg)
            }
        }
    }
    logger.AddGround(nSats + 0, 0.0, 0.0, "src");    // (kEarthR, 0, 0)
    logger.AddGround(nSats + 1, 0.0, 180.0, "dst");  // (-kEarthR, 0, 0)
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        logger.Connect(dtn.GetAgent(i));
    }

    // --- Traffic: ground station A -> ground station B ----------------------
    Ptr<BundleProtocolAgent> src = dtn.GetAgent(nSats + 0);
    EndpointId dst(nodes.Get(nSats + 1)->GetId(), 1);
    std::vector<uint8_t> payload(1024, 0xAB);
    for (uint32_t k = 0; k < nBundles; ++k)
    {
        const double t = 5.0 + k * (simTime - 20.0) / nBundles;
        Bundle::Priority prio = (k % 5 == 0) ? Bundle::EXPEDITED : Bundle::NORMAL;
        Simulator::Schedule(Seconds(t),
                            &BundleProtocolAgent::SendBundle,
                            src,
                            dst,
                            payload,
                            prio,
                            Seconds(4000.0));
    }

    // --- Runtime orbit driver ------------------------------------------------
    Simulator::Schedule(Seconds(0.0), &DriveOrbits, nodes, driveStep, simTime);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    logger.Write("dtn-leo-events.json"); // before Destroy(): timestamps use Simulator::Now()
    Simulator::Destroy();

    std::cout << "----------------------------------------\n";
    std::cout << "Bundles originated : " << nBundles << "\n";
    std::cout << "Bundles delivered  : " << delivered << "\n";
    std::cout << "Delivery ratio     : " << (100.0 * delivered / nBundles) << " %\n";
    std::cout << "Energy-deferrals   : " << deferred << "\n";
    std::cout << "Replay written to  : dtn-leo-events.json (load it in viz/dtn-leo-globe.html)\n";
    return 0;
}
