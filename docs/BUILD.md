# Build, Run & Validate

> It is written to build against **ns-3.48**. Expect to fix minor issues on first build inside your own tree.

## 1. Prerequisites

- ns-3.48 (`git checkout ns-3.48` of `gitlab.com/nsnam/ns-3-dev`)
- A C++17 compiler (GCC ≥ 9 or Clang ≥ 10), CMake ≥ 3.13, Python 3 — the same
  toolchain ns-3.48 already requires.
- No third-party libraries. The CBOR codec is self-contained; there is **no**
  dependency on ns3-ai, 5G-LENA, or the `leo` module to build and run the
  bundled example.

## 2. Install the module

```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev
git checkout ns-3.48

# drop the module into contrib/
cp -r /path/to/dtn-leo-ns3/contrib/dtn-leo contrib/dtn-leo
```

The module's public headers are exported under `ns3/` by `build_lib(...)`, so
source includes use the standard `#include "ns3/bp-bundle.h"` form and the
example includes the auto-generated aggregation header `ns3/dtn-leo-module.h`.
You do **not** hand-write `dtn-leo-module.h`; ns-3's build generates it from the
`HEADER_FILES` list in `contrib/dtn-leo/CMakeLists.txt`.

## 3. Configure & build

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

The module links only `core`, `network`, `internet`, `mobility`,
`applications`, and `csma` (see `contrib/dtn-leo/CMakeLists.txt`).

## 4. Run the example

```bash
./ns3 run "dtn-leo-cgr-example --verbose"
```

Expected console output: a generated contact count, then a per-run summary line
reporting **delivery ratio** and **energy-deferral count**, and a written
`dtn-leo-contact-plan.ion` file (ION-format contact plan) in the run directory.

Useful switches (see the top of `examples/dtn-leo-cgr-example.cc` for the full
list): number of orbital planes, satellites per plane, simulation duration,
bundle rate, and battery/eclipse parameters.

## 5. Run the tests

```bash
./test.py -s dtn-leo
# or a single suite run:
./ns3 run "test-runner --suite=dtn-leo"
```

Six cases: CBOR round-trip, bundle round-trip, CGR route selection, line-of-sight
occlusion, eclipse geometry, and SCF (store-carry-forward) policy.

## 6. Switching to the real ns-3.48 LEO mobility

The example ships with a self-contained **analytic circular-orbit oracle** so it
runs without the `leo` module. To use real orbits:

1. Enable/install the ns-3.48 `leo` module and build a constellation with the
   LEO node/orbit helpers.
2. Replace `OrbitOracle` in the example with a lambda that returns the LEO
   mobility model's position for `(nodeId, tSec)` — its propagator is
   deterministic, so the same query feeds both the contact-plan generator and
   the eclipse model.
3. Remove the example's runtime "orbit driver" loop (real LEO mobility updates
   node positions itself).

Nothing in `model/` or `helper/` changes — only the oracle you pass in. This is
the intended integration seam; the exact `leo` API calls are the one place you
should expect to adapt to your ns-3.48 checkout.

## 7. Cross-validating against ION-DTN (optional but recommended)

Because the module emits an ION-format contact plan and uses `ipn` EIDs, you can
feed the same contact plan to NASA JPL's ION and compare CGR route choices and
delivery outcomes. This is the strongest external validation available for the
routing contribution.

## 8. Troubleshooting first build

- **Header not found (`ns3/...`)**: confirm the file is listed under
  `HEADER_FILES` in `contrib/dtn-leo/CMakeLists.txt`; only listed headers are
  exported under `ns3/`.
- **Example not built**: you must pass `--enable-examples` at configure time.
- **Undefined `ns3::dtn::...`**: ensure `./ns3 build` rebuilt the `dtn-leo`
  library after adding the module (a fresh `./ns3 configure` forces re-scan).
- **TypeId assertion at runtime**: check that any new `Object` subclass you add
  registers a unique `GetTypeId()` name.
