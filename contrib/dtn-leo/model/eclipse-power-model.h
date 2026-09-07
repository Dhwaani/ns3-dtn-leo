/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_ECLIPSE_POWER_MODEL_H
#define DTN_LEO_ECLIPSE_POWER_MODEL_H

#include "ns3/mobility-model.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/vector.h"

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief Solar/eclipse-aware energy model for a LEO satellite node.
 *
 * A LEO satellite spends a large fraction of every ~90-minute orbit in Earth's
 * shadow, where its solar panels produce nothing and it must run off battery.
 * This model:
 *   1. Determines eclipse state geometrically from the node's position (taken
 *      from its MobilityModel) using a cylindrical Earth-shadow approximation
 *      relative to a configurable Sun direction.
 *   2. Integrates a battery state-of-charge: charging at \p m_solarChargeW in
 *      sunlight, discharging at a base housekeeping load plus any active
 *      transmit power.
 *
 * The store-carry-forward scheduler consults GetStateOfChargeFraction() and
 * IsInSunlight() to decide whether it is *energy-safe* to transmit a bundle
 * now, or whether the bundle should be carried until the next sunlit arc. This
 * "defer transmission across eclipse" behaviour is the project's core novelty
 * over classical CGR, which assumes contacts are always usable.
 *
 * Units are SI: positions in metres, energy in joules, power in watts.
 */
class EclipsePowerModel : public Object
{
  public:
    static TypeId GetTypeId();
    EclipsePowerModel();

    void SetMobility(Ptr<MobilityModel> mobility);

    /** \return true if the satellite currently receives sunlight. */
    bool IsInSunlight() const;
    bool IsInSunlight(Vector position) const;

    /** \return battery state of charge in [0,1]. */
    double GetStateOfChargeFraction() const;
    double GetStateOfChargeJoules() const;

    /** Notify the model that a transmission of \p bytes at \p rateBps starts. */
    void NotifyTransmitStart(double rateBps);
    void NotifyTransmitStop();

    /** Advance the battery integration to "now" (idempotent per timestep). */
    void UpdateSoc();

  private:
    double SolarInputW() const;

    Ptr<MobilityModel> m_mobility;

    // Geometry
    double m_earthRadiusM;   //!< Earth radius (m)
    Vector m_sunDirection;   //!< unit vector toward the Sun (ECI-ish, static approx)

    // Battery
    double m_capacityJoules; //!< battery capacity
    double m_socJoules;      //!< current stored energy
    double m_solarChargeW;   //!< charge power when sunlit
    double m_baseLoadW;      //!< always-on housekeeping load
    double m_txEfficiency;   //!< watts consumed per bps of transmit (very small)
    bool m_transmitting;
    double m_txRateBps;

    Time m_lastUpdate;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_ECLIPSE_POWER_MODEL_H */
