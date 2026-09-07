/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "contact-plan-generator.h"

#include "ns3/log.h"

#include <cmath>

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnContactPlanGenerator");

namespace
{
constexpr double SPEED_OF_LIGHT_M_S = 299792458.0;
}

ContactPlanGenerator::ContactPlanGenerator(PositionOracle oracle, Config cfg)
    : m_oracle(std::move(oracle)),
      m_cfg(cfg)
{
}

double
ContactPlanGenerator::Range(const Vector& a, const Vector& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool
ContactPlanGenerator::HasLineOfSight(const Vector& a, const Vector& b, double earthRadiusM)
{
    // Closest approach of the segment a--b to the Earth centre (origin).
    // Parametrize P(t) = a + t*(b-a), t in [0,1]. Minimize |P(t)|^2.
    const Vector d(b.x - a.x, b.y - a.y, b.z - a.z);
    const double dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (dd == 0.0)
    {
        return Range(a, Vector(0, 0, 0)) >= earthRadiusM;
    }
    double t = -(a.x * d.x + a.y * d.y + a.z * d.z) / dd;
    t = std::max(0.0, std::min(1.0, t));
    const Vector closest(a.x + t * d.x, a.y + t * d.y, a.z + t * d.z);
    const double minDist = std::sqrt(closest.x * closest.x + closest.y * closest.y +
                                     closest.z * closest.z);
    // Visible if the nearest point of the segment stays outside the Earth.
    // Small tolerance so a link anchored exactly on the surface (a ground
    // station at |r| == earthRadiusM) is not rejected by floating-point noise.
    return minDist >= earthRadiusM * 0.999;
}

ContactPlan
ContactPlanGenerator::Generate(const std::vector<uint32_t>& nodeIds) const
{
    ContactPlan plan;
    const std::size_t n = nodeIds.size();
    const int steps = static_cast<int>(std::floor(m_cfg.durationSec / m_cfg.stepSec)) + 1;

    // For each ordered pair, track an open contact and close it when visibility
    // drops or the horizon ends.
    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = 0; j < n; ++j)
        {
            if (i == j)
            {
                continue;
            }
            const uint32_t from = nodeIds[i];
            const uint32_t to = nodeIds[j];

            bool open = false;
            double contactStart = 0.0;
            double rangeAccum = 0.0;
            int rangeSamples = 0;

            for (int k = 0; k < steps; ++k)
            {
                const double t = k * m_cfg.stepSec;
                const Vector pa = m_oracle(from, t);
                const Vector pb = m_oracle(to, t);
                const double range = Range(pa, pb);

                bool visible = HasLineOfSight(pa, pb, m_cfg.earthRadiusM);
                if (visible && m_cfg.maxRangeM > 0.0 && range > m_cfg.maxRangeM)
                {
                    visible = false;
                }
                if (visible && m_cfg.minRangeM > 0.0 && range < m_cfg.minRangeM)
                {
                    visible = false;
                }

                if (visible && !open)
                {
                    open = true;
                    contactStart = t;
                    rangeAccum = range;
                    rangeSamples = 1;
                }
                else if (visible && open)
                {
                    rangeAccum += range;
                    ++rangeSamples;
                }
                else if (!visible && open)
                {
                    // Close the contact at the previous sample.
                    Contact c;
                    c.fromNode = from;
                    c.toNode = to;
                    c.startSec = contactStart;
                    c.endSec = (k - 1) * m_cfg.stepSec;
                    c.dataRateBps = m_cfg.defaultRateBps;
                    const double meanRange = rangeSamples ? rangeAccum / rangeSamples : range;
                    c.owltSec = meanRange / SPEED_OF_LIGHT_M_S;
                    if (c.endSec > c.startSec)
                    {
                        plan.AddContact(c);
                    }
                    open = false;
                }
            }

            // Close a contact still open at the horizon.
            if (open)
            {
                Contact c;
                c.fromNode = from;
                c.toNode = to;
                c.startSec = contactStart;
                c.endSec = (steps - 1) * m_cfg.stepSec;
                c.dataRateBps = m_cfg.defaultRateBps;
                const double meanRange = rangeSamples ? rangeAccum / rangeSamples : 0.0;
                c.owltSec = meanRange / SPEED_OF_LIGHT_M_S;
                plan.AddContact(c);
            }
        }
    }

    NS_LOG_INFO("Generated " << plan.Size() << " directed contacts among " << n << " nodes");
    return plan;
}

} // namespace dtn
} // namespace ns3
