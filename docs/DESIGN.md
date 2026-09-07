# Design

This document describes the architecture of the `dtn-leo` module: what each
class does, how a bundle flows through the system, and — most importantly — the
design decisions and simplifications a reviewer (or thesis committee) should be
aware of.

All classes live in `namespace ns3::dtn`.

## 1. Layered view

```
   Application intent  ──►  BundleProtocolAgent  ──►  CgrRouter  ──►  ContactPlan
   (SendBundle)             (custody store,           (earliest-       (contacts +
                             forwarding tick)          delivery          residual
                                   │                    Dijkstra)         volume)
                                   ▼
                             ScfScheduler  ◄── EclipsePowerModel (battery SOC, shadow)
                             (energy-aware
                              admission control)
                                   │
                                   ▼
                      UDP convergence-layer adapter  ──►  ns-3 sockets / IP / CSMA
                             (BpHeader on a Packet)
```

Node position (for both contact-plan generation and the eclipse model) is
supplied by a **position oracle** — a `std::function<Vector(uint32_t nodeId,
double tSec)>`. This is the single most important decoupling in the design (see
§4).

## 2. Class responsibilities

| Class | File | Role |
|---|---|---|
| `CborWriter` / `CborReader` | `model/cbor.*` | Minimal RFC 8949 CBOR codec (unsigned ints, byte/text strings, arrays). Enough for BPv7 block encoding; not a general CBOR library. |
| `EndpointId` | `model/bp-endpoint-id.*` | BPv7 `ipn` endpoint identifier (node, service). Chosen over `dtn:` URIs for compact CBOR and ION cross-compatibility. |
| `Bundle` | `model/bp-bundle.*` | BPv7 (RFC 9171) bundle: primary block + canonical/payload blocks, priority class (BULK / NORMAL / EXPEDITED), lifetime, `GetId()`. CBOR encode/decode. |
| `BpHeader` | `model/bp-header.*` | `ns3::Header` that carries a serialized bundle inside a `Packet` (4-byte length prefix + CBOR body). |
| `Contact`, `ContactPlan` | `model/contact-plan.*` | A time-bounded, directional link opportunity (from, to, start, end, rate, owlt) with residual-volume booking; ION contact-plan load/save. |
| `CgrRouter`, `CgrRoute` | `model/cgr-router.*` | Contact Graph Routing: earliest-delivery-time Dijkstra over contacts, honoring one-way light time, transmission time, residual volume, expiry, and an optional excluded first hop. |
| `EclipsePowerModel` | `model/eclipse-power-model.*` | `ns3::Object`. Cylindrical Earth-shadow test from node position; integrates battery state-of-charge (solar charge in sunlight, base load always, tx load on send). |
| `ScfScheduler` | `model/scf-scheduler.*` | `ns3::Object`. Power-aware admission control: `IsEnergySafeToSend()`, near-expiry / EXPEDITED overrides, `SelectReleasable()`, `PurgeExpired()`. |
| `BundleProtocolAgent` | `model/bundle-protocol-agent.*` | `ns3::Application`. UDP convergence layer (port 4556), custody store, periodic `ForwardingTick`, `TryForward` (CGR + contact-active check + energy gate), local delivery, traced sources. |
| `ContactPlanGenerator` | `helper/contact-plan-generator.*` | Turns the position oracle into a `ContactPlan`: samples positions, tests line-of-sight (Earth-sphere occlusion), computes owlt = range/c, coalesces visible samples into contacts. |
| `DtnLeoHelper` | `helper/dtn-leo-helper.*` | Installs the full stack per node: a shared `ContactPlan` + `CgrRouter`, and per-node `EclipsePowerModel` + `ScfScheduler` + `BundleProtocolAgent`. |

## 3. Bundle lifecycle

1. **Originate.** `BundleProtocolAgent::SendBundle(dest, payload, priority)` builds
   a `Bundle`, assigns it a lifetime, and places it in the custody store.
2. **Route.** On each `ForwardingTick`, for every stored bundle the agent calls
   `CgrRouter::ComputeRoute(src, dest, size, nowSec, excludeFirstHop)`. CGR runs
   an earliest-delivery-time search over the `ContactPlan`, returning a
   `CgrRoute` (next hop + predicted delivery time) or "no route".
3. **Gate on contact.** `ContactActiveNow(from, to, now)` confirms the first-hop
   contact is currently open (the contact plan, not L2 connectivity, decides
   when a link "exists").
4. **Gate on energy.** `ScfScheduler::IsEnergySafeToSend()` consults the
   `EclipsePowerModel`. If the satellite is near/into eclipse and the battery is
   below the release threshold, a non-urgent bundle is **deferred** (carried),
   not dropped — unless it is EXPEDITED or near expiry, which override.
5. **Transmit.** If both gates pass, the bundle is serialized via `BpHeader`
   into a `Packet` and sent over the UDP CLA to the next hop; `CgrRouter::
   BookRoute` decrements residual volume on the used contact.
6. **Custody / carry.** Until an onward contact is both open and energy-safe, the
   bundle waits in the store (store-carry-forward). `PurgeExpired` reclaims
   bundles past lifetime.
7. **Deliver.** At the destination, `DeliverLocally` fires the `Deliver` trace.

Traced sources (`Tx`, `Deliver`, `Defer`) let the example and tests measure
delivery ratio and energy-deferral count without instrumentation hooks in user
code.

## 4. Key design decisions & simplifications

These are deliberate and should be stated openly in the thesis.

- **Position oracle, not a hard `leo`-module dependency.** The contact-plan
  generator and eclipse model consume a `std::function` position oracle rather
  than calling the ns-3.48 `leo` API directly. This means the module builds and
  runs *today* with a plain analytic orbit (see the example) and later binds to
  the real LEO propagator by swapping in one lambda — no code changes in the
  DTN core. It also keeps unit tests hermetic. The cost is that orbit fidelity
  is only as good as the oracle you supply.

- **UDP convergence layer, single CSMA segment.** For IP reachability the
  example puts all nodes on one CSMA segment; the **contact plan**, not L2,
  decides when a link is usable (`ContactActiveNow`). This isolates the DTN
  logic (the thesis contribution) from link-layer modeling and keeps the example
  self-contained. A production successor would implement a proper per-ISL
  convergence-layer adapter over point-to-point/LEO channels. This is the main
  fidelity simplification and is flagged in `ROADMAP.md`.

- **Cylindrical (not conical) eclipse.** `EclipsePowerModel` uses a cylindrical
  umbra approximation (no penumbra). For LEO energy-budget purposes at thesis
  granularity this is standard and cheap; a conical shadow with penumbra is a
  drop-in refinement.

- **`ipn` EIDs and ION contact-plan format.** Using the `ipn` scheme and ION's
  contact-plan text format is a deliberate choice so results can be
  cross-validated against NASA JPL's ION-DTN (the reference implementation) — a
  validation path the author already has hands-on experience with.

- **Minimal CBOR.** The CBOR codec implements only the major types BPv7 blocks
  need. It is not a general-purpose CBOR library and intentionally rejects
  unsupported types rather than silently mis-decoding.

- **CGR scope.** The router implements the core earliest-delivery-time contact
  search with residual-volume and expiry handling. It does **not** implement the
  full ION CGR feature set (e.g., overbooking management, opportunistic/anticipated
  contacts, route caching). Those are named as future work, not claimed.

## 5. Quick Verification Checklist

1. `test/dtn-leo-test-suite.cc` — six unit cases (CBOR round-trip, bundle
   round-trip, CGR route selection, LOS occlusion, eclipse geometry, SCF policy).
2. `examples/dtn-leo-cgr-example.cc` — end-to-end delivery-ratio + energy-defer
   output over a small constellation; the analytic oracle is swappable for the
   real LEO propagator.
3. This document's §4 — the simplifications that bound the claims.
