/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_CGR_ROUTER_H
#define DTN_LEO_CGR_ROUTER_H

#include "contact-plan.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief The result of a CGR route computation.
 */
struct CgrRoute
{
    bool found{false};
    double bestDeliveryTimeSec{std::numeric_limits<double>::infinity()};
    uint32_t nextHopNode{0};           //!< first node to forward to
    std::vector<uint32_t> hopSequence; //!< source ... destination (node ids)
    std::vector<std::size_t> contactIndices; //!< indices into the ContactPlan, in order
};

/**
 * \ingroup dtn-leo
 * \brief Contact Graph Routing over a deterministic contact plan.
 *
 * This implements the essence of CGR's route-selection: an earliest-arrival
 * (best-delivery-time) search over the time-varying contact graph. For a
 * bundle of a given size, generated at \p currentTime with a given expiry, it
 * finds the route to \p dest that delivers the bundle at the earliest time,
 * honouring:
 *   - contact time windows [start, end],
 *   - one-way light time (owlt) per contact,
 *   - transmission time = bytes / rate (the whole bundle must fit before end),
 *   - residual contact volume (capacity minus already-booked bytes),
 *   - bundle expiry (routes delivering after expiry are rejected).
 *
 * The search is a Dijkstra variant whose node label is the earliest time the
 * bundle can be *present* at that node. This matches ION/HDTN CGR semantics at
 * the route-selection level (we omit route caching and yen-style alternates,
 * which are natural thesis extensions).
 *
 * \note An optional \p excludeNode set lets the caller implement CGR's
 *       "return-to-sender" suppression and per-neighbour route diversity.
 */
class CgrRouter
{
  public:
    explicit CgrRouter(ContactPlan* plan);

    /**
     * Compute the best-delivery-time route from \p source to \p dest.
     * \param bundleBytes  serialized bundle size (for transmission time + volume)
     * \param currentTimeSec  time the bundle is available at \p source
     * \param expiryTimeSec  absolute time after which the bundle is useless
     * \param excludeFirstHop  a node id to forbid as the first hop (0 = none)
     */
    CgrRoute ComputeRoute(uint32_t source,
                          uint32_t dest,
                          double bundleBytes,
                          double currentTimeSec,
                          double expiryTimeSec,
                          uint32_t excludeFirstHop = 0) const;

    /**
     * Book the volume of \p bundleBytes along the contacts of \p route so that
     * subsequent route computations see reduced residual capacity. Call this
     * once a bundle has actually been committed to a route.
     */
    void BookRoute(const CgrRoute& route, double bundleBytes);

  private:
    ContactPlan* m_plan; //!< not owned
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_CGR_ROUTER_H */
