/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "scf-scheduler.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnScfScheduler");
NS_OBJECT_ENSURE_REGISTERED(ScfScheduler);

TypeId
ScfScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::dtn::ScfScheduler")
            .SetParent<Object>()
            .SetGroupName("DtnLeo")
            .AddConstructor<ScfScheduler>()
            .AddAttribute("ReleaseSocFraction",
                          "Minimum battery state-of-charge (fraction) required to release "
                          "a normal/bulk bundle for transmission.",
                          DoubleValue(0.40),
                          MakeDoubleAccessor(&ScfScheduler::m_releaseSocFraction),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("UrgentExpiryGuardSeconds",
                          "If a bundle is within this many seconds of expiry, transmit it "
                          "regardless of energy state.",
                          DoubleValue(60.0),
                          MakeDoubleAccessor(&ScfScheduler::m_urgentExpiryGuardSec),
                          MakeDoubleChecker<double>())
            .AddAttribute("AllowEclipseForExpedited",
                          "Whether EXPEDITED-priority bundles may transmit during eclipse.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&ScfScheduler::m_allowEclipseForExpedited),
                          MakeBooleanChecker());
    return tid;
}

ScfScheduler::ScfScheduler()
    : m_releaseSocFraction(0.40),
      m_urgentExpiryGuardSec(60.0),
      m_allowEclipseForExpedited(true)
{
}

void
ScfScheduler::SetPowerModel(Ptr<EclipsePowerModel> power)
{
    m_power = power;
}

void
ScfScheduler::Enqueue(const Bundle& bundle)
{
    m_store.push_back(bundle);
}

std::size_t
ScfScheduler::StoreSize() const
{
    return m_store.size();
}

const std::deque<Bundle>&
ScfScheduler::GetStore() const
{
    return m_store;
}

bool
ScfScheduler::IsEnergySafeToSend(const Bundle& b, double nowSec) const
{
    // Urgency override: near-expiry bundles always go, energy be damned.
    const double expirySec =
        (b.primary.creationTimestampTime + b.primary.lifetimeMs) / 1000.0;
    if (expirySec - nowSec <= m_urgentExpiryGuardSec)
    {
        return true;
    }

    if (!m_power)
    {
        return true; // no power model attached -> behave like plain CGR
    }

    const bool sunlit = m_power->IsInSunlight();
    const double soc = m_power->GetStateOfChargeFraction();

    // Expedited traffic (e.g. telecommand, alerts) may run even in eclipse.
    if (b.priority == Bundle::EXPEDITED)
    {
        return m_allowEclipseForExpedited ? true : sunlit;
    }

    // Normal / bulk: only when sunlit OR battery comfortably above threshold.
    if (sunlit)
    {
        return true;
    }
    return soc >= m_releaseSocFraction;
}

std::vector<Bundle>
ScfScheduler::SelectReleasable(double nowSec) const
{
    std::vector<Bundle> releasable;
    for (const auto& b : m_store)
    {
        if (IsEnergySafeToSend(b, nowSec))
        {
            releasable.push_back(b);
        }
    }
    // Order: higher priority first, then nearest expiry first.
    std::sort(releasable.begin(),
              releasable.end(),
              [](const Bundle& x, const Bundle& y) {
                  if (x.priority != y.priority)
                  {
                      return x.priority > y.priority;
                  }
                  const double ex =
                      x.primary.creationTimestampTime + x.primary.lifetimeMs;
                  const double ey =
                      y.primary.creationTimestampTime + y.primary.lifetimeMs;
                  return ex < ey;
              });
    return releasable;
}

bool
ScfScheduler::RemoveById(const std::string& id)
{
    for (auto it = m_store.begin(); it != m_store.end(); ++it)
    {
        if (it->GetId() == id)
        {
            m_store.erase(it);
            return true;
        }
    }
    return false;
}

std::size_t
ScfScheduler::PurgeExpired(double nowSec)
{
    std::size_t dropped = 0;
    for (auto it = m_store.begin(); it != m_store.end();)
    {
        const double expirySec =
            (it->primary.creationTimestampTime + it->primary.lifetimeMs) / 1000.0;
        if (expirySec <= nowSec)
        {
            it = m_store.erase(it);
            ++dropped;
        }
        else
        {
            ++it;
        }
    }
    if (dropped)
    {
        NS_LOG_INFO("SCF purged " << dropped << " expired bundles");
    }
    return dropped;
}

} // namespace dtn
} // namespace ns3
