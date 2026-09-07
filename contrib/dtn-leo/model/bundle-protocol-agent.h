/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_BUNDLE_PROTOCOL_AGENT_H
#define DTN_LEO_BUNDLE_PROTOCOL_AGENT_H

#include "bp-bundle.h"
#include "cgr-router.h"
#include "contact-plan.h"
#include "eclipse-power-model.h"
#include "scf-scheduler.h"

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <map>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief A Bundle Protocol v7 agent running on one node.
 *
 * Responsibilities:
 *  - hosts one endpoint id (ipn:node.service),
 *  - stores bundles in a power-aware SCF scheduler,
 *  - forwards bundles using CGR over a shared contact plan, honouring both the
 *    contact schedule and the eclipse/battery admission control,
 *  - acts as a UDP-based convergence layer between nodes.
 *
 * All agents share one ContactPlan and one CgrRouter (owned by the helper), so
 * volume bookings are globally consistent. Each agent owns its own store,
 * scheduler and power model.
 *
 * Traced sources let the example/tests measure delivery ratio, latency, and
 * how many transmissions were deferred by the energy policy.
 */
class BundleProtocolAgent : public Application
{
  public:
    static TypeId GetTypeId();
    BundleProtocolAgent();
    ~BundleProtocolAgent() override;

    /// \name Traced-source callback signatures (see the m_*Trace members).
    /// @{
    typedef void (*TxTracedCallback)(std::string id, uint32_t fromNode, uint32_t toNode, uint8_t priority);
    typedef void (*DeliverTracedCallback)(std::string id, uint32_t atNode);
    typedef void (*DeferTracedCallback)(std::string id, uint32_t atNode, uint8_t priority);
    /// @}

    void Setup(EndpointId eid,
               ContactPlan* plan,
               CgrRouter* router,
               Ptr<ScfScheduler> scheduler,
               Ptr<EclipsePowerModel> power);

    /** Register the IP address that reaches a given node id (convergence layer). */
    void SetNodeAddress(uint32_t nodeId, Ipv4Address addr);

    /** Originate a bundle from this node to \p dest. */
    void SendBundle(EndpointId dest,
                    const std::vector<uint8_t>& payload,
                    Bundle::Priority priority,
                    Time lifetime);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleRead(Ptr<Socket> socket);
    void ForwardingTick();
    void TryForward(const Bundle& b);
    bool ContactActiveNow(uint32_t from, uint32_t to, double nowSec) const;
    void DeliverLocally(const Bundle& b);

    EndpointId m_eid;
    ContactPlan* m_plan{nullptr};   //!< shared, not owned
    CgrRouter* m_router{nullptr};   //!< shared, not owned
    Ptr<ScfScheduler> m_scheduler;  //!< owned per-node
    Ptr<EclipsePowerModel> m_power; //!< owned per-node

    Ptr<Socket> m_socket;
    uint16_t m_port;
    std::map<uint32_t, Ipv4Address> m_nodeAddr;

    Time m_tickInterval;
    EventId m_tickEvent;
    uint64_t m_seq{0};

    // Tracing. Signatures are purpose-built for external logging/visualization
    // (e.g. the JSON trace logger that feeds the dtn-leo-globe replay view):
    //   Tx      : (bundleId, fromNode, toNode, priority)
    //   Deliver : (bundleId, atNode)
    //   Defer   : (bundleId, atNode, priority)  -- energy-carried this tick
    TracedCallback<std::string, uint32_t, uint32_t, uint8_t> m_txTrace;
    TracedCallback<std::string, uint32_t> m_deliverTrace;
    TracedCallback<std::string, uint32_t, uint8_t> m_deferTrace;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_BUNDLE_PROTOCOL_AGENT_H */
