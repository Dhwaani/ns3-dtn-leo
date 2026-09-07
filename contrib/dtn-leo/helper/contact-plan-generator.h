/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_CONTACT_PLAN_GENERATOR_H
#define DTN_LEO_CONTACT_PLAN_GENERATOR_H

#include "ns3/contact-plan.h"
#include "ns3/vector.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Derive a deterministic DTN contact plan from LEO orbital geometry.
 *
 * This is the piece that connects ns-3.48's LEO mobility to DTN routing, which
 * — to our knowledge — has not been done before in ns-3. Given a position
 * *oracle* p(nodeId, t) that returns each node's ECI/ECEF position at any
 * future time, the generator:
 *   1. samples all node positions on a fixed time grid,
 *   2. tests pairwise line-of-sight visibility, blocking the link when the
 *      straight segment is occluded by a spherical Earth (radius Re), and,
 *      for ground links, when the satellite is below a minimum elevation,
 *   3. coalesces consecutive visible samples into contact intervals,
 *   4. assigns each contact a data rate and a one-way light time
 *      (owlt = midpoint-range / c).
 *
 * The position oracle is intentionally decoupled from any specific mobility
 * class: for the ns-3 `leo` module you pass a lambda that calls the LEO
 * propagator; for tests you pass an analytic function; for constant-velocity
 * mobility the helper can build a linear-extrapolation oracle.
 */
class ContactPlanGenerator
{
  public:
    /** Signature of the position oracle: (nodeId, timeSeconds) -> position (m). */
    using PositionOracle = std::function<Vector(uint32_t, double)>;

    struct Config
    {
        double durationSec{6000.0};   //!< horizon to generate contacts over
        double stepSec{5.0};          //!< sampling granularity
        double earthRadiusM{6.371e6}; //!< occlusion sphere radius
        double defaultRateBps{10e6};  //!< link data rate assigned to contacts
        double minRangeM{0.0};        //!< ignore contacts closer than this (0 = off)
        double maxRangeM{5.0e6};      //!< ignore links longer than this (0 = off)
    };

    ContactPlanGenerator(PositionOracle oracle, Config cfg);

    /**
     * Generate contacts among the given node ids. Bidirectional contacts are
     * emitted as two directed contacts (i->j and j->i).
     */
    ContactPlan Generate(const std::vector<uint32_t>& nodeIds) const;

    /** Line-of-sight test: is the segment a--b unobstructed by the Earth sphere? */
    static bool HasLineOfSight(const Vector& a, const Vector& b, double earthRadiusM);

    /** Range (m) between two positions. */
    static double Range(const Vector& a, const Vector& b);

  private:
    PositionOracle m_oracle;
    Config m_cfg;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_CONTACT_PLAN_GENERATOR_H */
