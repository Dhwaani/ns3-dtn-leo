/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "dtn-leo-helper.h"

#include "ns3/eclipse-power-model.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/names.h"
#include "ns3/scf-scheduler.h"

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnLeoHelper");

DtnLeoHelper::DtnLeoHelper()
    : m_plan(std::make_shared<ContactPlan>()),
      m_router(std::make_shared<CgrRouter>(nullptr))
{
    // Rebuild router bound to our plan.
    m_router = std::make_shared<CgrRouter>(m_plan.get());
}

void
DtnLeoHelper::SetContactPlan(const ContactPlan& plan)
{
    *m_plan = plan;
    m_router = std::make_shared<CgrRouter>(m_plan.get());
}

ContactPlan&
DtnLeoHelper::GetContactPlan()
{
    return *m_plan;
}

CgrRouter&
DtnLeoHelper::GetRouter()
{
    return *m_router;
}

ApplicationContainer
DtnLeoHelper::Install(NodeContainer nodes, const std::vector<Ipv4Address>& ifaces)
{
    NS_ASSERT_MSG(ifaces.size() == nodes.GetN(),
                  "ifaces must be index-aligned with nodes");
    ApplicationContainer apps;
    m_agents.clear();

    // Build the node-id -> address map once (shared logically by all agents).
    std::map<uint32_t, Ipv4Address> addrMap;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        addrMap[nodes.Get(i)->GetId()] = ifaces[i];
    }

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);

        // Per-node power model bound to the node's mobility.
        Ptr<EclipsePowerModel> power = CreateObject<EclipsePowerModel>();
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        if (mob)
        {
            power->SetMobility(mob);
        }

        Ptr<ScfScheduler> sched = CreateObject<ScfScheduler>();

        Ptr<BundleProtocolAgent> agent = CreateObject<BundleProtocolAgent>();
        agent->Setup(EndpointId(node->GetId(), 1), m_plan.get(), m_router.get(), sched, power);
        for (const auto& kv : addrMap)
        {
            agent->SetNodeAddress(kv.first, kv.second);
        }

        node->AddApplication(agent);
        apps.Add(agent);
        m_agents.push_back(agent);
    }

    NS_LOG_INFO("Installed DTN stack on " << nodes.GetN() << " nodes; contact plan has "
                                          << m_plan->Size() << " contacts");
    return apps;
}

Ptr<BundleProtocolAgent>
DtnLeoHelper::GetAgent(uint32_t i) const
{
    return i < m_agents.size() ? m_agents[i] : nullptr;
}

} // namespace dtn
} // namespace ns3
