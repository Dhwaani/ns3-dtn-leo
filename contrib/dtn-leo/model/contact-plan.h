/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DTN_LEO_CONTACT_PLAN_H
#define DTN_LEO_CONTACT_PLAN_H

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace dtn
{

/**
 * \ingroup dtn-leo
 * \brief A scheduled communication opportunity between two nodes.
 *
 * Mirrors the ION "contact" abstraction: a directed edge that is usable only
 * during [start, end], with a fixed data rate and a one-way light time (owlt)
 * derived from the inter-node range at contact midpoint. Contacts are the
 * vertices of the contact graph that CGR searches.
 */
struct Contact
{
    uint32_t fromNode{0};
    uint32_t toNode{0};
    double startSec{0.0};
    double endSec{0.0};
    double dataRateBps{0.0};
    double owltSec{0.0};      //!< one-way light time (range / c)

    // Mutable routing state (booked volume for residual-capacity accounting):
    double bookedBytes{0.0};

    double DurationSec() const;
    /** Maximum bytes transferable over the whole contact window. */
    double CapacityBytes() const;
    /** Remaining bytes after current bookings. */
    double ResidualBytes() const;
};

/**
 * \ingroup dtn-leo
 * \brief An ordered set of contacts: the deterministic schedule CGR routes on.
 *
 * The plan can be produced three ways:
 *   1. Generated from ns-3 LEO mobility (see ContactPlanGenerator) — the novel path.
 *   2. Loaded from an ION-style contact plan file (for cross-validation).
 *   3. Constructed programmatically in tests.
 */
class ContactPlan
{
  public:
    void AddContact(const Contact& c);
    const std::vector<Contact>& GetContacts() const;
    std::vector<Contact>& GetContactsMutable();
    std::size_t Size() const;
    void Clear();

    /** Reset all volume bookings (e.g. between routing runs). */
    void ResetBookings();

    /**
     * Load an ION-format contact plan (subset): lines of the form
     *   a contact +<start> +<end> <from> <to> <rateBytesPerSec>
     *   a range   +<start> +<end> <from> <to> <owltSec>
     * Comments (#) and blank lines are ignored. Returns number of contacts read.
     */
    std::size_t LoadFromIonFile(const std::string& path);

    /** Write the plan in the same ION-style format (for diffing vs ION). */
    void SaveToIonFile(const std::string& path) const;

  private:
    std::vector<Contact> m_contacts;
};

} // namespace dtn
} // namespace ns3

#endif /* DTN_LEO_CONTACT_PLAN_H */
