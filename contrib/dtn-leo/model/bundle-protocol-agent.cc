/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "bundle-protocol-agent.h"

#include "bp-header.h"

#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnBundleProtocolAgent");
NS_OBJECT_ENSURE_REGISTERED(BundleProtocolAgent);

TypeId
BundleProtocolAgent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::dtn::BundleProtocolAgent")
            .SetParent<Application>()
            .SetGroupName("DtnLeo")
            .AddConstructor<BundleProtocolAgent>()
            .AddAttribute("Port",
                          "UDP port used by the convergence layer.",
                          UintegerValue(4556), // IANA bundle port
                          MakeUintegerAccessor(&BundleProtocolAgent::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("ForwardingTick",
                          "Interval between store scans/forwarding attempts.",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&BundleProtocolAgent::m_tickInterval),
                          MakeTimeChecker())
            .AddTraceSource("Tx",
                            "A bundle was transmitted on a convergence-layer hop.",
                            MakeTraceSourceAccessor(&BundleProtocolAgent::m_txTrace),
                            "ns3::dtn::BundleProtocolAgent::TxTracedCallback")
            .AddTraceSource("Deliver",
                            "A bundle reached its destination endpoint.",
                            MakeTraceSourceAccessor(&BundleProtocolAgent::m_deliverTrace),
                            "ns3::dtn::BundleProtocolAgent::DeliverTracedCallback")
            .AddTraceSource("Defer",
                            "A bundle transmission was deferred by the energy policy.",
                            MakeTraceSourceAccessor(&BundleProtocolAgent::m_deferTrace),
                            "ns3::dtn::BundleProtocolAgent::DeferTracedCallback");
    return tid;
}

BundleProtocolAgent::BundleProtocolAgent()
    : m_port(4556),
      m_tickInterval(Seconds(1.0))
{
}

BundleProtocolAgent::~BundleProtocolAgent()
{
}

void
BundleProtocolAgent::Setup(EndpointId eid,
                           ContactPlan* plan,
                           CgrRouter* router,
                           Ptr<ScfScheduler> scheduler,
                           Ptr<EclipsePowerModel> power)
{
    m_eid = eid;
    m_plan = plan;
    m_router = router;
    m_scheduler = scheduler;
    m_power = power;
    if (m_scheduler)
    {
        m_scheduler->SetPowerModel(m_power);
    }
}

void
BundleProtocolAgent::SetNodeAddress(uint32_t nodeId, Ipv4Address addr)
{
    m_nodeAddr[nodeId] = addr;
}

void
BundleProtocolAgent::DoDispose()
{
    m_socket = nullptr;
    m_scheduler = nullptr;
    m_power = nullptr;
    Application::DoDispose();
}

void
BundleProtocolAgent::StartApplication()
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
        m_socket->SetRecvCallback(MakeCallback(&BundleProtocolAgent::HandleRead, this));
    }
    m_tickEvent = Simulator::Schedule(m_tickInterval, &BundleProtocolAgent::ForwardingTick, this);
    NS_LOG_INFO(m_eid.ToString() << " started");
}

void
BundleProtocolAgent::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    Simulator::Cancel(m_tickEvent);
}

void
BundleProtocolAgent::SendBundle(EndpointId dest,
                                const std::vector<uint8_t>& payload,
                                Bundle::Priority priority,
                                Time lifetime)
{
    Bundle b;
    b.primary.source = m_eid;
    b.primary.destination = dest;
    b.primary.reportTo = m_eid;
    b.primary.creationTimestampTime = static_cast<uint64_t>(Simulator::Now().GetMilliSeconds());
    b.primary.creationTimestampSeq = m_seq++;
    b.primary.lifetimeMs = static_cast<uint64_t>(lifetime.GetMilliSeconds());
    b.priority = priority;
    b.enqueuedAtSec = Simulator::Now().GetSeconds();
    b.SetPayload(payload);
    b.sizeBytesHint = b.GetSerializedSizeBytes();

    NS_LOG_INFO(m_eid.ToString() << " originating bundle " << b.GetId() << " -> "
                                 << dest.ToString());
    m_scheduler->Enqueue(b);
    // Attempt immediate forwarding on the next tick (keeps logic in one place).
}

bool
BundleProtocolAgent::ContactActiveNow(uint32_t from, uint32_t to, double nowSec) const
{
    if (!m_plan)
    {
        return true;
    }
    for (const auto& c : m_plan->GetContacts())
    {
        if (c.fromNode == from && c.toNode == to && c.startSec <= nowSec && nowSec <= c.endSec)
        {
            return true;
        }
    }
    return false;
}

void
BundleProtocolAgent::ForwardingTick()
{
    const double now = Simulator::Now().GetSeconds();
    if (m_power)
    {
        m_power->UpdateSoc();
    }
    m_scheduler->PurgeExpired(now);

    // Bundles cleared by the energy policy, highest priority first.
    std::vector<Bundle> releasable = m_scheduler->SelectReleasable(now);
    for (const auto& b : releasable)
    {
        TryForward(b);
    }

    // Surface energy-carried bundles for tracing/visualization: any stored
    // bundle the energy policy will not release right now is being carried
    // across an eclipse / low-battery arc rather than forwarded.
    for (const auto& b : m_scheduler->GetStore())
    {
        if (!m_scheduler->IsEnergySafeToSend(b, now))
        {
            m_deferTrace(b.GetId(),
                         static_cast<uint32_t>(m_eid.GetNode()),
                         static_cast<uint8_t>(b.priority));
        }
    }

    m_tickEvent = Simulator::Schedule(m_tickInterval, &BundleProtocolAgent::ForwardingTick, this);
}

void
BundleProtocolAgent::TryForward(const Bundle& b)
{
    const double now = Simulator::Now().GetSeconds();
    const uint32_t myNode = m_eid.GetNode();
    const uint32_t destNode = b.primary.destination.GetNode();

    // Already home?
    if (destNode == myNode)
    {
        DeliverLocally(b);
        m_scheduler->RemoveById(b.GetId());
        return;
    }

    const double expiry =
        (b.primary.creationTimestampTime + b.primary.lifetimeMs) / 1000.0;
    const double bytes = static_cast<double>(b.GetSerializedSizeBytes());

    CgrRoute route = m_router->ComputeRoute(myNode, destNode, bytes, now, expiry);
    if (!route.found)
    {
        NS_LOG_LOGIC(m_eid.ToString() << " no CGR route for " << b.GetId() << "; carrying");
        return; // carry — try again next tick
    }

    const uint32_t nextHop = route.nextHopNode;

    // Store-carry-forward: only transmit if the first-hop contact is open *now*.
    if (!ContactActiveNow(myNode, nextHop, now))
    {
        NS_LOG_LOGIC(m_eid.ToString() << " next-hop contact to " << nextHop
                                      << " not open yet; carrying " << b.GetId());
        return;
    }

    auto addrIt = m_nodeAddr.find(nextHop);
    if (addrIt == m_nodeAddr.end())
    {
        NS_LOG_WARN(m_eid.ToString() << " no address for next hop " << nextHop);
        return;
    }

    // Commit: book volume, transmit, remove from store.
    m_router->BookRoute(route, bytes);

    BpHeader header(b);
    Ptr<Packet> pkt = Create<Packet>();
    pkt->AddHeader(header);

    if (m_power)
    {
        m_power->NotifyTransmitStart(0.0); // rate accounted at model default
    }
    m_socket->SendTo(pkt, 0, InetSocketAddress(addrIt->second, m_port));
    if (m_power)
    {
        m_power->NotifyTransmitStop();
    }

    m_txTrace(b.GetId(),
              static_cast<uint32_t>(myNode),
              static_cast<uint32_t>(nextHop),
              static_cast<uint8_t>(b.priority));
    m_scheduler->RemoveById(b.GetId());
    NS_LOG_INFO(m_eid.ToString() << " forwarded " << b.GetId() << " to node " << nextHop);
}

void
BundleProtocolAgent::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> pkt;
    Address from;
    while ((pkt = socket->RecvFrom(from)))
    {
        BpHeader header;
        pkt->RemoveHeader(header);
        const Bundle& b = header.GetBundle();

        if (b.primary.destination.GetNode() == m_eid.GetNode())
        {
            DeliverLocally(b);
        }
        else
        {
            // Intermediate custody: store and forward later.
            Bundle carried = b;
            carried.enqueuedAtSec = Simulator::Now().GetSeconds();
            m_scheduler->Enqueue(carried);
            NS_LOG_INFO(m_eid.ToString() << " accepted custody of " << b.GetId());
        }
    }
}

void
BundleProtocolAgent::DeliverLocally(const Bundle& b)
{
    NS_LOG_INFO(m_eid.ToString() << " DELIVERED " << b.GetId() << " ("
                                 << b.GetPayload().size() << " payload bytes)");
    m_deliverTrace(b.GetId(), static_cast<uint32_t>(m_eid.GetNode()));
}

} // namespace dtn
} // namespace ns3
