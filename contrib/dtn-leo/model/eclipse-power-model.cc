/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */

#include "eclipse-power-model.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/vector.h"

#include <cmath>

namespace ns3
{
namespace dtn
{

NS_LOG_COMPONENT_DEFINE("DtnEclipsePowerModel");
NS_OBJECT_ENSURE_REGISTERED(EclipsePowerModel);

TypeId
EclipsePowerModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::dtn::EclipsePowerModel")
            .SetParent<Object>()
            .SetGroupName("DtnLeo")
            .AddConstructor<EclipsePowerModel>()
            .AddAttribute("EarthRadius",
                          "Earth radius used for the cylindrical shadow test (m).",
                          DoubleValue(6.371e6),
                          MakeDoubleAccessor(&EclipsePowerModel::m_earthRadiusM),
                          MakeDoubleChecker<double>())
            .AddAttribute("BatteryCapacityJoules",
                          "Battery capacity (J).",
                          DoubleValue(3.0e4),
                          MakeDoubleAccessor(&EclipsePowerModel::m_capacityJoules),
                          MakeDoubleChecker<double>())
            .AddAttribute("SolarChargeWatts",
                          "Solar array output while sunlit (W).",
                          DoubleValue(20.0),
                          MakeDoubleAccessor(&EclipsePowerModel::m_solarChargeW),
                          MakeDoubleChecker<double>())
            .AddAttribute("BaseLoadWatts",
                          "Always-on housekeeping load (W).",
                          DoubleValue(5.0),
                          MakeDoubleAccessor(&EclipsePowerModel::m_baseLoadW),
                          MakeDoubleChecker<double>());
    return tid;
}

EclipsePowerModel::EclipsePowerModel()
    : m_earthRadiusM(6.371e6),
      m_sunDirection(1.0, 0.0, 0.0),
      m_capacityJoules(3.0e4),
      m_socJoules(3.0e4),
      m_solarChargeW(20.0),
      m_baseLoadW(5.0),
      m_txEfficiency(2.0e-8), // ~ watts per bps; tune per radio
      m_transmitting(false),
      m_txRateBps(0.0),
      m_lastUpdate(Seconds(0))
{
}

void
EclipsePowerModel::SetMobility(Ptr<MobilityModel> mobility)
{
    m_mobility = mobility;
}

bool
EclipsePowerModel::IsInSunlight(Vector p) const
{
    // Normalize sun direction.
    const double sMag =
        std::sqrt(m_sunDirection.x * m_sunDirection.x + m_sunDirection.y * m_sunDirection.y +
                  m_sunDirection.z * m_sunDirection.z);
    if (sMag == 0.0)
    {
        return true;
    }
    const Vector s(m_sunDirection.x / sMag, m_sunDirection.y / sMag, m_sunDirection.z / sMag);

    // Component of position along the sun direction.
    const double along = p.x * s.x + p.y * s.y + p.z * s.z;

    // If the satellite is on the sunward side of Earth, it is always sunlit.
    if (along >= 0.0)
    {
        return true;
    }

    // Otherwise it is in shadow only if within the anti-solar cylinder of
    // radius = Earth radius, i.e. its perpendicular distance to the
    // Earth-Sun axis is smaller than Re.
    const Vector perp(p.x - along * s.x, p.y - along * s.y, p.z - along * s.z);
    const double perpDist = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
    return perpDist > m_earthRadiusM;
}

bool
EclipsePowerModel::IsInSunlight() const
{
    if (!m_mobility)
    {
        return true;
    }
    return IsInSunlight(m_mobility->GetPosition());
}

double
EclipsePowerModel::SolarInputW() const
{
    return IsInSunlight() ? m_solarChargeW : 0.0;
}

void
EclipsePowerModel::UpdateSoc()
{
    const Time now = Simulator::Now();
    const double dt = (now - m_lastUpdate).GetSeconds();
    if (dt <= 0.0)
    {
        return;
    }
    m_lastUpdate = now;

    double loadW = m_baseLoadW;
    if (m_transmitting)
    {
        loadW += m_txEfficiency * m_txRateBps;
    }
    const double netW = SolarInputW() - loadW;
    m_socJoules += netW * dt;
    m_socJoules = std::max(0.0, std::min(m_capacityJoules, m_socJoules));
}

double
EclipsePowerModel::GetStateOfChargeFraction() const
{
    return m_capacityJoules > 0.0 ? m_socJoules / m_capacityJoules : 0.0;
}

double
EclipsePowerModel::GetStateOfChargeJoules() const
{
    return m_socJoules;
}

void
EclipsePowerModel::NotifyTransmitStart(double rateBps)
{
    UpdateSoc();
    m_transmitting = true;
    m_txRateBps = rateBps;
}

void
EclipsePowerModel::NotifyTransmitStop()
{
    UpdateSoc();
    m_transmitting = false;
    m_txRateBps = 0.0;
}

} // namespace dtn
} // namespace ns3
