/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_HELPER_H
#define DTN_LEO_HELPER_H

#include "ns3/application-container.h"
#include "ns3/bundle-protocol-agent.h"
#include "ns3/cgr-router.h"
#include "ns3/contact-plan.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"

#include <map>
#include <memory>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Installs a complete BPv7 + CGR + eclipse-aware SCF stack on nodes.
 *
 * The helper owns the single shared ContactPlan and CgrRouter (so volume
 * bookings are consistent across the constellation) and, for each node,
 * creates an EclipsePowerModel, an ScfScheduler, and a BundleProtocolAgent
 * with endpoint id ipn:<nodeId>.1.
 *
 * Typical use:
 * \code
 *   DtnLeoHelper dtn;
 *   dtn.SetContactPlan(plan);                 // from ContactPlanGenerator
 *   ApplicationContainer apps = dtn.Install(nodes, ifaces);
 *   apps.Start(Seconds(0)); apps.Stop(Seconds(6000));
 * \endcode
 */
class DtnLeoHelper
{
  public:
    DtnLeoHelper();

    /** Provide the contact plan the whole constellation will route on. */
    void SetContactPlan(const ContactPlan& plan);

    ContactPlan& GetContactPlan();
    CgrRouter& GetRouter();

    /**
     * Install the DTN stack on \p nodes. \p ifaces must give the IPv4 address
     * that reaches each node (index-aligned with \p nodes) for the UDP
     * convergence layer. Returns the created agents as an ApplicationContainer.
     */
    ApplicationContainer Install(NodeContainer nodes, const std::vector<Ipv4Address>& ifaces);

    /** \return the agent installed on node index \p i (for scripting traffic). */
    Ptr<BundleProtocolAgent> GetAgent(uint32_t i) const;

  private:
    std::shared_ptr<ContactPlan> m_plan;
    std::shared_ptr<CgrRouter> m_router;
    std::vector<Ptr<BundleProtocolAgent>> m_agents;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_HELPER_H */
