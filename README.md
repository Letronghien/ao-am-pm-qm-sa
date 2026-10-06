# ao-am-pm-qm-sa

ns-3.48 implementations of AOMDV, PMAODV, QMAODV and SA-QMAODV.
AODV is the `aodv` module of ns-3.48 (unmodified).

| Protocol | Module | Helper |
|---|---|---|
| AODV | `aodv` (ns-3.48) | `AodvHelper` |
| AOMDV | `aomdv` | `AomdvHelper` |
| PMAODV | `mpaodv` | `PmaodvHelper(k)`, k = 2, 3, 4 |
| QMAODV | `mpaodv` | `QmaodvHelper` |
| SA-QMAODV | `mpaodv` | `SaQmaodvHelper` |

## Install

```
git clone https://gitlab.com/nsnam/ns-3-dev.git -b ns-3.48 ns-3.48
cp -r aomdv mpaodv ns-3.48/contrib/
cd ns-3.48
./ns3 configure --enable-examples -d optimized
./ns3 build
./ns3 run "manet-routing-example --protocol=SA-QMAODV --nNodes=20"
```

## Use

```cpp
#include "ns3/aomdv-module.h"
#include "ns3/mpaodv-module.h"

InternetStackHelper stack;
QmaodvHelper qmaodv;          // or AomdvHelper, PmaodvHelper(3), SaQmaodvHelper
stack.SetRoutingHelper(qmaodv);
stack.Install(nodes);
```

## Parameters

| | PMAODV | QMAODV | SA-QMAODV |
|---|---|---|---|
| Routes per destination | k | 3 | 3 |
| Next-hop selection | p_i ∝ 1/HC_i | ε-greedy on Q | ε-greedy on Q |
| Initial Q | — | (1/HC_i) / Σ_k (1/HC_k) | (1/HC_i) / Σ_k (1/HC_k) |
| α | — | 0.5 | 0.1 + 0.8 (1 − e^(−0.1 ΔSeq)), ΔSeq in 10 s |
| γ | — | 0.9 | 0.9 |
| ε | — | 0.5, −0.02 every 10 s | 0.3, +0.2 on RERR (max 0.5), −0.02 every 5 s (min 0.1) |
| Reward | — | 0.6 ACK + 0.4 / (1 + d_ms) | w1 ACK + w2 / (1 + d_ms) + w3 E; (0.5, 0.4, 0.1), (0.1, 0.1, 0.8) below 20 % energy |

All three use the same discovery (alternate reverse routes from duplicate RREQs, multicast RREP)
and select a next hop at every node for data packets.

## References

- T.-H. Le, K. Tran Thi-Minh, H.-D. Ngo, "Probabilistic Multipath Ad-hoc Routing Protocol for the Internet of Things based Applications," LNNS 1782, Springer, 2026, pp. 287–299.
- T.-H. Le, K. Tran Thi-Minh, H.-D. Ngo, "QMAODV: A Q-Learning-Based Multipath Routing Protocol for UAV-Enabled Ad Hoc IoT Networks," LNDECT 281, Springer, 2026, pp. 147–162.
- T.-H. Le, K. Tran Thi-Minh, H.-D. Ngo, "Self-Adaptive Q-Learning for Multipath Routing in Dynamic UAV Ad Hoc Networks."
- M. K. Marina, S. R. Das, "On-demand multipath distance vector routing in ad hoc networks," ICNP 2001 (AOMDV; port of the ns-2.35 code).

## License

GPL-2.0-only.
