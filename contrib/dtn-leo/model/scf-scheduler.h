/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_SCF_SCHEDULER_H
#define DTN_LEO_SCF_SCHEDULER_H

#include "bp-bundle.h"
#include "eclipse-power-model.h"

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <deque>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Power-aware store-carry-forward (SCF) scheduler.
 *
 * Classical CGR/BP forwards a bundle as soon as a contact to the CGR-chosen
 * next hop opens. On battery-constrained LEO satellites that is often the
 * wrong thing to do: transmitting during an eclipse arc, when the battery is
 * low, can starve mission-critical housekeeping and force safe-mode.
 *
 * This scheduler adds an *energy admission control* layer on top of the store:
 * a bundle is released for transmission only if it is energy-safe, unless the
 * bundle is high priority or close to expiry, in which case delivery urgency
 * overrides energy conservation. Otherwise the bundle is *carried* until the
 * next sunlit arc or until the battery recovers above a release threshold.
 *
 * The policy is deliberately simple and inspectable so its effect can be
 * isolated in the evaluation (delivery ratio / latency vs. energy safety
 * violations) against a plain-CGR baseline.
 */
class ScfScheduler : public Object
{
  public:
    static TypeId GetTypeId();
    ScfScheduler();

    void SetPowerModel(Ptr<EclipsePowerModel> power);

    /** Add a bundle to the store. */
    void Enqueue(const Bundle& bundle);

    /** \return number of bundles currently stored. */
    std::size_t StoreSize() const;

    /** Peek the whole store (ordered as stored). */
    const std::deque<Bundle>& GetStore() const;

    /**
     * Decide, for a given stored bundle, whether it may be transmitted at
     * \p nowSec given the current power state. Encodes the core novelty.
     */
    bool IsEnergySafeToSend(const Bundle& b, double nowSec) const;

    /**
     * Return the ordered list of bundles cleared for transmission now,
     * highest priority / nearest expiry first, filtered by energy safety.
     * Bundles are NOT removed here; the caller removes on successful send.
     */
    std::vector<Bundle> SelectReleasable(double nowSec) const;

    /** Remove a bundle from the store by id (after a successful forward). */
    bool RemoveById(const std::string& id);

    /** Drop expired bundles; returns how many were dropped. */
    std::size_t PurgeExpired(double nowSec);

  private:
    Ptr<EclipsePowerModel> m_power; //!< energy state source

    // Tunables (exposed as ns-3 attributes):
    double m_releaseSocFraction; //!< battery must be >= this to send normal bundles
    double m_urgentExpiryGuardSec; //!< within this margin of expiry, send regardless
    bool m_allowEclipseForExpedited; //!< expedited bundles may transmit in eclipse

    std::deque<Bundle> m_store;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_SCF_SCHEDULER_H */
