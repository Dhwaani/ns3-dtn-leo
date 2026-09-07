# dtn-leo — Delay-Tolerant Networking over LEO Constellations for ns-3

An open-source **ns-3.48** contrib module that brings **Bundle Protocol v7
(RFC 9171)**, **Contact Graph Routing (CGR)**, and **eclipse-aware,
power-constrained store-carry-forward** to Low-Earth-Orbit satellite networks.

## Why this exists

ns-3.48 (June 2026) added LEO **mobility** and ray-traced propagation, but ns-3
still has **no working DTN**: its only Bundle Protocol is an unmerged 2013
RFC 5050 (BPv6) prototype, with no BPv7, no CGR, and no link to the new LEO
models. This module fills that gap and targets the 6G non-terrestrial-network
(NTN) research wave.

## Novelty

1. **BPv7/CBOR bundle model** for ns-3 (RFC 9171 primary/canonical blocks,
   `ipn` EIDs) — a modernization of the abandoned BPv6 code.
2. **CGR over ns-3 LEO geometry** — a contact-plan generator turns orbital
   line-of-sight + light-time into a deterministic contact plan, and an
   earliest-delivery-time CGR engine routes bundles over it. Coupling CGR to
   ns-3's LEO mobility appears to be previously unpublished.
3. **Eclipse-aware power-constrained SCF** — a satellite battery/eclipse model
   drives an admission-control scheduler that *carries* non-urgent bundles
   across shadow arcs instead of forwarding blindly, trading a little latency
   for energy safety. This is the differentiating research question.

See [`docs/NOVELTY.md`](docs/NOVELTY.md) and [`docs/LITERATURE.md`](docs/LITERATURE.md).

## Repository layout

```
dtn-leo-ns3/
├── contrib/dtn-leo/           # the ns-3 module (drop into <ns-3-dev>/contrib/)
│   ├── model/                 # CBOR, BPv7 bundle, CGR, contact plan, power, SCF, agent
│   ├── helper/                # contact-plan generator + install helper
│   ├── examples/              # dtn-leo-cgr-example.cc
│   ├── test/                  # ns-3 unit test suite
│   ├── doc/                   # module .rst
│   └── CMakeLists.txt
├── docs/                      # DESIGN, BUILD
└── LICENSE                    # GPL-2.0-only (matches ns-3)
```

## Quick start

```bash
# 1. Get ns-3.48
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev && git checkout ns-3.48

# 2. Add this module
cp -r /path/to/dtn-leo-ns3/contrib/dtn-leo contrib/dtn-leo

# 3. Configure + build
./ns3 configure --enable-examples --enable-tests
./ns3 build

# 4. Run
./ns3 run "dtn-leo-cgr-example --verbose"
./test.py -s dtn-leo
```

Full instructions, prerequisites, and the LEO-module integration path are in
[`docs/BUILD.md`](docs/BUILD.md).

## Visualization

`viz/dtn-leo-globe.html` is a self-contained 3D orbital-network console: the
constellation orbiting Earth, contacts opening and closing, satellites passing
through eclipse, and bundles hopping ground→sat→sat→ground — with eclipse-aware
carrying shown directly (a low-battery satellite in shadow *holds* a non-urgent
bundle instead of forwarding it). Each bundle's CGR route lights up before it is
flown: solid for the hops already taken, pale cyan for the planned remainder.
Open the HTML file in a browser to run the built-in analytic constellation, or
load a JSON export of a real ns-3 run to replay it — the example writes
`dtn-leo-events.json` automatically via the header-only `JsonTraceLogger`. Data
schema and trace hooks are in [`viz/README.md`](viz/README.md);
[`viz/sample-data.json`](viz/sample-data.json) is a ready-to-load example.

## License

GPL-2.0-only, matching ns-3. See [`LICENSE`](LICENSE).

## Citing / prior art

This work builds on the Bundle Protocol (RFC 9171, Burleigh/Fall/Birrane),
Contact Graph Routing (Burleigh/Fraire/Caini), and ns-3's LEO mobility
lineage.
