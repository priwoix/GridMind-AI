# GridMind AI — Technical Report

## Executive summary

GridMind AI is a C++17 proof-of-concept for coordinating energy use across five EV-charging metro clusters. It simulates hourly demand and renewable availability, compares a baseline dispatch policy with a battery-aware heuristic, and exports operational KPIs.

The prototype is designed to demonstrate the control logic and provide a foundation for data integration. It does **not** establish that the operator's stated reliability, loss or EBITDA targets have been achieved. All results are scenario outputs generated from illustrative assumptions.

## 1. Problem framing

The operator's challenge combines several sources of uncertainty:
- EV charging demand varies by hour and by metro cluster.
- Solar and wind availability changes over the day.
- Market procurement prices vary by time.
- Storage has power, energy, efficiency and reserve constraints.
- Poorly timed procurement can create avoidable costs, while inadequate dispatchable energy can create unserved demand.

The prototype focuses on the hourly energy-dispatch part of this problem.

## 2. System representation

| Element | Representation |
|---|---|
| Geography | Five metro clusters |
| Installed capacity | 1,200 MW total, represented as 240 MW per cluster |
| Storage | 600 MWh total, represented as 120 MWh per cluster |
| Battery power | 40 MW maximum per cluster |
| Mandatory battery reserve | 5% SOC |
| Operating target | 20% SOC |
| Efficiency | 95% |
| Grid procurement | 150 MW/hour/cluster illustrative cap |
| Time horizon | 24 hourly intervals |

The 5% system reserve obligation corresponds to 60 MW of installed capacity. In this prototype it is reported explicitly, but a separate generator dispatch model is not yet implemented.

## 3. Demand and renewable profiles

Demand is generated from each cluster's base demand and a common hourly multiplier:
- 1.15 during the morning period (06:00–09:00)
- 1.30 during the evening peak (17:00–21:00)
- 0.65 overnight (22:00–04:00)
- 0.85 during other hours

Renewable output is represented using a simple solar availability curve and a bounded sinusoidal wind profile. The profile assumes a 45% solar / 55% wind mix. These are synthetic patterns, not a meteorological forecast.

## 4. Dispatch algorithms

### 4.1 Baseline

For every cluster and hour:
1. Use renewable generation to serve demand.
2. Buy remaining energy from the grid up to the illustrative 150 MW hourly limit.
3. Record any remaining demand as unserved.

No battery is used in the baseline.

### 4.2 GridMind heuristic

For every cluster and hour:
1. Use renewable generation first.
2. If the market price is high or remaining demand exceeds the grid limit, discharge storage.
3. Preserve the 5% mandatory SOC reserve.
4. During a shortage, permit emergency dispatch below the 20% operating target, but not below the 5% reserve.
5. Procure remaining energy from the grid, subject to the hourly limit.
6. Store renewable surplus when battery room and power are available.
7. During low-price hours, charge from the grid if procurement headroom remains and SOC is below 80%.

The strategy is intentionally readable. It is a rule-based heuristic, not a proof of global optimality.

## 5. Tariff constraint

The customer tariff signal is bounded between 80% and 120% of the previous day's average tariff. The prototype uses an illustrative previous-day average of ₹6.00/kWh, giving a band of ₹4.80–₹7.20/kWh.

The tariff is reported as a constrained signal. The current version does not model customer price elasticity or calculate a full demand-response equilibrium.

## 6. KPIs

The program calculates:
- Total demand and energy served
- Unserved energy
- Reliability
- Renewable energy utilized and curtailed
- Grid procurement
- Battery charge and discharge throughput
- Estimated energy procurement cost
- Reliability change and estimated cost saving versus baseline

Reliability is:

`100 × (Total demand − Unserved energy) / Total demand`

Estimated energy cost includes grid energy at the hourly market price and, for the optimized case, an illustrative battery degradation cost per MWh discharged. It excludes renewable contract costs, network charges, taxes, demand charges, capex, opex and financing.

## 7. Interpretation

The generated output should be interpreted as a controlled experiment:
- If reliability increases, the battery heuristic served more demand under the assumed local grid cap.
- If cost falls, the assumed time-of-use procurement and battery dispatch reduced modeled energy cost after the degradation adder.
- If cost rises, the reliability improvement may have been purchased at higher modeled energy cost.
- Renewable curtailment indicates available renewable energy that could not be directly used or stored under the current battery constraints.

The numerical outcome depends heavily on synthetic demand, renewable availability, market prices, starting SOC and the assumed grid cap. It must not be presented as a measured forecast or guaranteed operator outcome.

## 8. Gaps against the full challenge

The brief describes a broader operating platform. This prototype is a first dispatch engine, not the entire production system.

| Challenge requirement | Current status |
|---|---|
| Five metro clusters | Implemented |
| 1,200 MW installed capacity | Represented |
| 600 MWh storage | Represented |
| 5% battery SOC reserve | Implemented |
| 5% system capacity reserve | Calculated/reported; separate reserve dispatch not implemented |
| Hourly price-aware dispatch | Implemented with synthetic prices |
| 35% renewable procurement share | Not enforced over an annual horizon |
| 60% DC fast / 40% swap mix | Not yet modeled as separate service demand |
| Swap-pack inventory and stock-outs | Not yet implemented |
| Forecasting | Synthetic profile only; no trained forecast model |
| Physical/network portfolio balancing | Not implemented |
| Dynamic tariff ±20% | Constraint implemented as a tariff signal |
| Battery SOH | Simplified throughput-based indicator |
| Reliability target ~89.7% | Not claimed; must be evaluated on real data |
| Loss reduction 10% to 8% | Not modeled |
| EBITDA margin >12% | Not modeled; financial inputs are missing |

## 9. Validation plan

Before making performance claims:
1. Obtain the five years of hourly cluster demand, weather, renewable output, IEX prices, charging logs and battery SOH.
2. Separate training, validation and test periods chronologically.
3. Compare demand and renewable forecasts against simple baselines.
4. Replay the same historical days through baseline and GridMind dispatch.
5. Enforce reserve, grid, battery power, battery energy and tariff constraints at every interval.
6. Report reliability, unserved MWh, renewable utilization, procurement cost, battery throughput and constraint violations.
7. Run sensitivity tests for demand spikes, forecast errors, low renewable output and battery degradation.
8. Build an EBITDA model only after tariffs, revenue, operating costs, penalties and capital costs are available.

## 10. Conclusion

GridMind AI demonstrates a working, explainable starting point for multi-cluster EV energy dispatch. Its main value at this stage is the simulation framework, explicit operating constraints, baseline comparison and exportable KPIs. The next major step is replacing synthetic inputs with real historical data and then upgrading the heuristic into a forecast-driven constrained optimization model.
