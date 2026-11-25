# GridMind AI — EV Energy Optimization Prototype

A C++17 prototype that simulates energy dispatch for five Indian EV-charging clusters over 24 hours. It compares a simple baseline against a rule-based optimization strategy.

## 1. What this project does

- Simulates five metro clusters: Delhi NCR, Mumbai MMR, Bengaluru, Hyderabad and Chennai.
- Models hourly EV charging demand, solar/wind availability and IEX-style market prices.
- Compares baseline dispatch with GridMind's battery-aware dispatch.
- Prioritizes renewable energy, uses batteries during expensive/shortage periods, and charges storage when energy is cheap or renewable energy is in surplus.
- Keeps a mandatory 5% battery state-of-charge reserve.
- Applies an illustrative customer-tariff band of ±20% around the previous day's average tariff.
- Tracks reliability, unserved energy, renewable use/curtailment, grid procurement, battery throughput and estimated energy cost.
- Exports CSV files for further analysis.

## 2. Requirements

- A C++17 compiler (g++ recommended).
- Windows, Linux or macOS.
- No third-party libraries.

## 3. Compile and run

Open a terminal in the project folder.

### Windows PowerShell (MinGW g++)

```powershell
g++ -std=c++17 -O2 src/main.cpp -o gridmind.exe
.\gridmind.exe
```

### Linux / macOS

```bash
g++ -std=c++17 -O2 src/main.cpp -o gridmind
./gridmind
```

The program writes:
- `outputs/hourly_dispatch.csv`
- `outputs/kpi_comparison.csv`

Run the program from the project root so the `outputs` folder is found.

## 4. Project structure

```text
GridMind_AI_CPP/
├── src/
│   └── main.cpp
├── data/
│   └── DATA_NOTES.md
├── outputs/
│   └── .gitkeep
├── README.md
├── TECHNICAL_REPORT.md
└── .gitignore
```

## 5. Important model assumptions

This is a hackathon prototype. The current build uses synthetic hourly profiles rather than the operator's five years of actual data.

| Parameter | Prototype assumption |
|---|---:|
| Metro clusters | 5 |
| Installed capacity | 1,200 MW |
| Installed capacity per cluster | 240 MW (equal allocation) |
| Storage | 600 MWh total / 120 MWh per cluster |
| Battery power limit | 40 MW per cluster |
| Battery mandatory SOC floor | 5% |
| Operational SOC target | 20% |
| Charge/discharge efficiency | 95% |
| Grid procurement cap | 150 MW per cluster per hour |
| Renewable mix used in profile | 45% solar / 55% wind |
| Renewable contract share in brief | 35% (not yet enforced as an annual energy constraint) |
| System capacity reserve | 5% of 1,200 MW = 60 MW |
| Customer tariff constraint | ±20% of previous-day average |

The 150 MW per-cluster grid limit, battery power, starting SOC, hourly demand shapes, renewable capacities and prices are illustrative. Replace them with measured/contracted values before using the model for operational decisions.

## 6. What “optimization” means here

The dispatch engine is a transparent heuristic:
1. Serve demand from available renewable generation.
2. Discharge the battery during expensive hours or when local grid capacity is insufficient.
3. Keep at least 5% SOC untouched.
4. Use emergency battery energy below the 20% operating target only when needed to reduce unserved demand.
5. Charge from renewable surplus.
6. Charge from the grid during low-price hours when there is grid headroom.

This is not a mathematical optimum. A later version could replace the heuristic with a linear/mixed-integer optimization model and forecast inputs.

## 7. Current limitations

- One synthetic day (24 hourly steps), not five years of historical validation.
- No real weather, IEX, demand, battery SOH or swap-station logs are loaded.
- No physical inter-city power transfer is simulated.
- No detailed 60% DC fast-charging / 40% battery-swap service queue model yet.
- The 5% system capacity reserve is reported as a policy obligation; the prototype does not dispatch a separate generator fleet.
- The 35% renewable procurement share is described in the scenario but is not yet enforced as an annual procurement constraint.
- No EBITDA calculation: revenue, capex, opex, demand charges, penalties and financing data are not supplied.
- Battery degradation is represented by a simple throughput cost and illustrative SOH decrement.

## 8. How to read the outputs

`kpi_comparison.csv` contains one row per metric and two columns for baseline and optimized results.

`hourly_dispatch.csv` contains one row per cluster per hour. Compare baseline and optimized columns to see when the battery reduced grid demand or unserved energy.

Reliability is calculated as:

```text
Reliability (%) = 100 × (Demand − Unserved Energy) / Demand
```

A positive cost saving means the estimated optimized energy cost is lower than the baseline. If the saving is negative, the strategy improved another metric at a higher energy cost.

## 9. Suggested next development steps

1. Load real hourly CSV data instead of generating synthetic profiles.
2. Add demand and renewable forecasting with train/test evaluation.
3. Add swap-pack inventory, queue and stock-out simulation.
4. Enforce the 35% renewable share across the planning horizon.
5. Model network constraints and portfolio balancing using explicit grid transfer assumptions.
6. Replace heuristic dispatch with an optimization solver.
7. Add a validated financial model before reporting EBITDA impact.
