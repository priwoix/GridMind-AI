/*
    GRIDMIND AI - EV ENERGY OPTIMIZATION PROTOTYPE
    C++17 | Synthetic 24-hour scenario | No external libraries

    This is a transparent heuristic simulator, not a production grid optimizer.
    All scenario values are illustrative and should be replaced with operator data.
*/

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace std;

// ------------------------------ CONFIGURATION ------------------------------

constexpr int HOURS = 24;
constexpr double TOTAL_INSTALLED_CAPACITY_MW = 1200.0;
constexpr double SYSTEM_RESERVE_FRACTION = 0.05;
constexpr double TOTAL_STORAGE_MWH = 600.0;
constexpr double BATTERY_RESERVE_FRACTION = 0.05;
constexpr double OPERATIONAL_SOC_TARGET = 0.20;
constexpr double BATTERY_EFFICIENCY = 0.95;
constexpr double BATTERY_MAX_POWER_MW = 40.0;
constexpr double GRID_LIMIT_MW_PER_CLUSTER = 150.0;
constexpr double DEGRADATION_COST_PER_MWH = 350.0;
constexpr double PREVIOUS_DAY_AVG_TARIFF = 6.00; // INR/kWh
constexpr double TARIFF_FLOOR = PREVIOUS_DAY_AVG_TARIFF * 0.80;
constexpr double TARIFF_CEILING = PREVIOUS_DAY_AVG_TARIFF * 1.20;

struct Cluster {
    string name;
    double baseDemandMW;
    double renewableCapacityMW;
    double batteryCapacityMWh;
    double initialSocMWh;
};

struct Battery {
    double capacityMWh;
    double socMWh;
    double reserveMWh;
    double maxPowerMW;
    double stateOfHealth = 100.0;
};

struct HourResult {
    int hour = 0;
    string cluster;
    double demand = 0.0;
    double renewableAvailable = 0.0;
    double renewableUsed = 0.0;
    double renewableCurtailed = 0.0;
    double gridEnergy = 0.0;
    double batteryDischarge = 0.0;
    double batteryCharge = 0.0;
    double unserved = 0.0;
    double iexPrice = 0.0;
    double customerTariff = 0.0;
    double batterySoc = 0.0;
};

struct Metrics {
    double demand = 0.0;
    double served = 0.0;
    double unserved = 0.0;
    double renewableUsed = 0.0;
    double renewableCurtailed = 0.0;
    double gridProcurement = 0.0;
    double batteryCharged = 0.0;
    double batteryDischarged = 0.0;
    double totalCost = 0.0;
    double tariffRevenue = 0.0;

    double reliability() const {
        return demand > 0.0 ? 100.0 * served / demand : 0.0;
    }
};

// ------------------------------ SCENARIO DATA -------------------------------

vector<Cluster> createClusters() {
    // Five equal 240 MW installed-capacity clusters sum to 1,200 MW.
    // Demand and renewable capacities are illustrative scenario inputs.
    return {
        {"Delhi_NCR", 225.0, 100.0, 120.0, 80.0},
        {"Mumbai_MMR", 200.0,  90.0, 120.0, 75.0},
        {"Bengaluru",  180.0,  80.0, 120.0, 80.0},
        {"Hyderabad",  145.0,  70.0, 120.0, 65.0},
        {"Chennai",    125.0,  60.0, 120.0, 70.0}
    };
}

double demandMultiplier(int hour) {
    if (hour >= 6 && hour <= 9) return 1.15;   // morning activity
    if (hour >= 17 && hour <= 21) return 1.30; // evening peak
    if (hour >= 22 || hour <= 4) return 0.65;  // overnight
    return 0.85;
}

double solarAvailability(int hour) {
    if (hour < 6 || hour > 18) return 0.0;
    return max(0.0, 1.0 - abs(hour - 12) / 7.0);
}

double windAvailability(int hour) {
    double value = 0.50 + 0.20 * sin(hour * 0.55) + 0.10 * cos(hour * 0.31);
    return clamp(value, 0.15, 0.90);
}

double renewableGeneration(const Cluster& c, int hour) {
    // Illustrative 45% solar / 55% wind mix.
    double availability = 0.45 * solarAvailability(hour)
                        + 0.55 * windAvailability(hour);
    return c.renewableCapacityMW * availability;
}

double iexPrice(int hour) {
    if (hour >= 17 && hour <= 21) return 9.50;
    if (hour >= 10 && hour <= 15) return 3.00;
    if (hour <= 5) return 3.50;
    if (hour >= 6 && hour <= 9) return 6.50;
    return 5.50;
}

double constrainedCustomerTariff(double marketPrice, int hour) {
    // Illustrative time-of-use tariff signal, constrained to +/-20%
    // of the previous day's average tariff.
    double raw = marketPrice;
    if (hour >= 17 && hour <= 21) raw = PREVIOUS_DAY_AVG_TARIFF * 1.15;
    else if (hour >= 10 && hour <= 15) raw = PREVIOUS_DAY_AVG_TARIFF * 0.90;
    else raw = PREVIOUS_DAY_AVG_TARIFF;

    return clamp(raw, TARIFF_FLOOR, TARIFF_CEILING);
}

Battery createBattery(const Cluster& c) {
    Battery b;
    b.capacityMWh = c.batteryCapacityMWh;
    b.socMWh = c.initialSocMWh;
    b.reserveMWh = c.batteryCapacityMWh * BATTERY_RESERVE_FRACTION;
    b.maxPowerMW = BATTERY_MAX_POWER_MW;
    return b;
}

// ------------------------------ DISPATCH LOGIC ------------------------------

HourResult runBaselineHour(const Cluster& c, int hour) {
    HourResult r;
    r.hour = hour;
    r.cluster = c.name;
    r.demand = c.baseDemandMW * demandMultiplier(hour);
    r.renewableAvailable = renewableGeneration(c, hour);
    r.iexPrice = iexPrice(hour);
    r.customerTariff = constrainedCustomerTariff(r.iexPrice, hour);

    r.renewableUsed = min(r.demand, r.renewableAvailable);
    double remaining = r.demand - r.renewableUsed;
    r.gridEnergy = min(remaining, GRID_LIMIT_MW_PER_CLUSTER);
    remaining -= r.gridEnergy;
    r.unserved = max(0.0, remaining);
    r.renewableCurtailed = max(0.0, r.renewableAvailable - r.renewableUsed);
    return r;
}

HourResult runOptimizedHour(const Cluster& c, Battery& battery, int hour) {
    HourResult r;
    r.hour = hour;
    r.cluster = c.name;
    r.demand = c.baseDemandMW * demandMultiplier(hour);
    r.renewableAvailable = renewableGeneration(c, hour);
    r.iexPrice = iexPrice(hour);
    r.customerTariff = constrainedCustomerTariff(r.iexPrice, hour);

    // Step 1: serve demand from available renewable generation first.
    r.renewableUsed = min(r.demand, r.renewableAvailable);
    double remaining = r.demand - r.renewableUsed;
    double renewableSurplus = max(0.0, r.renewableAvailable - r.renewableUsed);

    // Step 2: use storage during expensive hours or when grid capacity is tight.
    // The normal target is 20% SOC. In a genuine supply shortfall, emergency
    // dispatch may go below 20%, but never below the mandatory 5% reserve.
    bool expensiveHour = (r.iexPrice >= 8.0);
    bool likelyGridShortage = (remaining > GRID_LIMIT_MW_PER_CLUSTER);
    if (expensiveHour || likelyGridShortage) {
        double normalAvailable = max(0.0, battery.socMWh
                                      - battery.capacityMWh * OPERATIONAL_SOC_TARGET);
        double emergencyAvailable = max(0.0, battery.socMWh - battery.reserveMWh);
        double availableToDischarge = (likelyGridShortage
                                    ? emergencyAvailable
                                    : normalAvailable) * BATTERY_EFFICIENCY;
        double discharge = min({remaining, battery.maxPowerMW, availableToDischarge});
        r.batteryDischarge = discharge;
        // Battery output is delivered energy; SOC falls by output / efficiency.
        battery.socMWh -= discharge / BATTERY_EFFICIENCY;
        battery.socMWh = max(battery.reserveMWh, battery.socMWh);
        remaining -= discharge;
        battery.stateOfHealth = max(80.0, battery.stateOfHealth
                                    - (discharge * 0.00002));
    }

    // Step 3: procure from the grid, respecting the illustrative cluster limit.
    r.gridEnergy = min(remaining, GRID_LIMIT_MW_PER_CLUSTER);
    remaining -= r.gridEnergy;

    // Step 4: if there is still unmet demand, make one emergency battery attempt.
    // This covers cases where the first dispatch was limited by the 20% target.
    if (remaining > 1e-9) {
        double available = max(0.0, battery.socMWh - battery.reserveMWh);
        double emergencyDischarge = min({remaining, battery.maxPowerMW
                                         - r.batteryDischarge, available
                                         * BATTERY_EFFICIENCY});
        r.batteryDischarge += emergencyDischarge;
        battery.socMWh -= emergencyDischarge / BATTERY_EFFICIENCY;
        battery.socMWh = max(battery.reserveMWh, battery.socMWh);
        remaining -= emergencyDischarge;
    }

    r.unserved = max(0.0, remaining);

    // Step 5: store renewable surplus. Charge input is greater than SOC gain
    // because round-trip losses are represented through charge efficiency.
    double room = max(0.0, battery.capacityMWh - battery.socMWh);
    double chargeFromRenewable = min({renewableSurplus, battery.maxPowerMW, room
                                      / BATTERY_EFFICIENCY});
    r.batteryCharge = chargeFromRenewable;
    battery.socMWh += chargeFromRenewable * BATTERY_EFFICIENCY;
    renewableSurplus -= chargeFromRenewable;

    // Step 6: optionally charge from cheap grid energy if headroom remains.
    // This is a heuristic price-arbitrage rule, not an optimized market bid.
    if (r.iexPrice <= 3.5 && battery.socMWh
        < battery.capacityMWh * 0.80) {
        double gridHeadroom = max(0.0, GRID_LIMIT_MW_PER_CLUSTER - r.gridEnergy);
        double chargeRoom = max(0.0, battery.capacityMWh - battery.socMWh);
        double gridCharge = min({gridHeadroom, battery.maxPowerMW
                                 - r.batteryCharge, chargeRoom / BATTERY_EFFICIENCY});
        if (gridCharge > 0.0) {
            r.batteryCharge += gridCharge;
            r.gridEnergy += gridCharge;
            battery.socMWh += gridCharge * BATTERY_EFFICIENCY;
        }
    }

    r.renewableCurtailed = max(0.0, renewableSurplus);
    r.batterySoc = battery.socMWh;
    return r;
}

// ------------------------------ KPI ACCOUNTING ------------------------------

void addToMetrics(Metrics& m, const HourResult& r, bool optimized) {
    m.demand += r.demand;
    m.unserved += r.unserved;
    m.served += r.demand - r.unserved;
    m.renewableUsed += r.renewableUsed;
    m.renewableCurtailed += r.renewableCurtailed;
    m.gridProcurement += r.gridEnergy;
    m.batteryCharged += r.batteryCharge;
    m.batteryDischarged += r.batteryDischarge;

    // Grid energy is in MWh for each one-hour interval.
    m.totalCost += r.gridEnergy * 1000.0 * r.iexPrice;
    if (optimized) {
        m.totalCost += r.batteryDischarge * DEGRADATION_COST_PER_MWH;
    }
    m.tariffRevenue += (r.demand - r.unserved) * 1000.0 * r.customerTariff;
}

void writeHourlyCsv(const vector<HourResult>& baseline,
                    const vector<HourResult>& optimized) {
    ofstream out("outputs/hourly_dispatch.csv");
    out << "hour,cluster,baseline_demand_mwh,baseline_renewable_used_mwh,"
           "baseline_grid_mwh,baseline_unserved_mwh,optimized_demand_mwh,"
           "optimized_renewable_used_mwh,optimized_grid_mwh,"
           "optimized_battery_discharge_mwh,optimized_battery_charge_mwh,"
           "optimized_unserved_mwh,optimized_battery_soc_mwh,"
           "iex_price_inr_per_kwh,customer_tariff_inr_per_kwh\n";

    for (size_t i = 0; i < baseline.size(); ++i) {
        const auto& b = baseline[i];
        const auto& o = optimized[i];
        out << b.hour << ',' << b.cluster << ','
            << b.demand << ',' << b.renewableUsed << ',' << b.gridEnergy << ','
            << b.unserved << ',' << o.demand << ',' << o.renewableUsed << ','
            << o.gridEnergy << ',' << o.batteryDischarge << ','
            << o.batteryCharge << ',' << o.unserved << ',' << o.batterySoc << ','
            << o.iexPrice << ',' << o.customerTariff << '\n';
    }
}

void writeKpiCsv(const Metrics& baseline, const Metrics& optimized) {
    ofstream out("outputs/kpi_comparison.csv");
    out << "metric,baseline,gridmind_optimized,unit\n";
    out << "demand," << baseline.demand << ',' << optimized.demand << ",MWh\n";
    out << "served_energy," << baseline.served << ',' << optimized.served << ",MWh\n";
    out << "unserved_energy," << baseline.unserved << ',' << optimized.unserved << ",MWh\n";
    out << "reliability," << baseline.reliability() << ',' << optimized.reliability() << ",percent\n";
    out << "renewable_utilized," << baseline.renewableUsed << ',' << optimized.renewableUsed << ",MWh\n";
    out << "renewable_curtailed," << baseline.renewableCurtailed << ',' << optimized.renewableCurtailed << ",MWh\n";
    out << "grid_procurement," << baseline.gridProcurement << ',' << optimized.gridProcurement << ",MWh\n";
    out << "battery_charged," << baseline.batteryCharged << ',' << optimized.batteryCharged << ",MWh\n";
    out << "battery_discharged," << baseline.batteryDischarged << ',' << optimized.batteryDischarged << ",MWh\n";
    out << "total_energy_cost," << baseline.totalCost << ',' << optimized.totalCost << ",INR\n";
}

void printMetrics(const string& label, const Metrics& m) {
    cout << "\n========== " << label << " ==========\n";
    cout << fixed << setprecision(2);
    cout << left << setw(30) << "Total demand (MWh)" << m.demand << '\n';
    cout << left << setw(30) << "Energy served (MWh)" << m.served << '\n';
    cout << left << setw(30) << "Unserved energy (MWh)" << m.unserved << '\n';
    cout << left << setw(30) << "Reliability (%)" << m.reliability() << '\n';
    cout << left << setw(30) << "Renewable used (MWh)" << m.renewableUsed << '\n';
    cout << left << setw(30) << "Renewable curtailed (MWh)" << m.renewableCurtailed << '\n';
    cout << left << setw(30) << "Grid procurement (MWh)" << m.gridProcurement << '\n';
    cout << left << setw(30) << "Battery charged (MWh)" << m.batteryCharged << '\n';
    cout << left << setw(30) << "Battery discharged (MWh)" << m.batteryDischarged << '\n';
    cout << left << setw(30) << "Estimated energy cost (INR)" << m.totalCost << '\n';
}

int main() {
    auto clusters = createClusters();

    // 5% of 1,200 MW = 60 MW held as system capacity reserve.
    const double systemReserveMW =
        TOTAL_INSTALLED_CAPACITY_MW * SYSTEM_RESERVE_FRACTION;

    cout << "============================================\n";
    cout << "              GRIDMIND AI                   \n";
    cout << "      EV ENERGY OPTIMIZATION SYSTEM         \n";
    cout << "============================================\n";
    cout << "Clusters: " << clusters.size() << '\n';
    cout << "Installed capacity: " << TOTAL_INSTALLED_CAPACITY_MW << " MW\n";
    cout << "System reserve obligation: " << systemReserveMW << " MW\n";
    cout << "Storage capacity: " << TOTAL_STORAGE_MWH << " MWh\n";
    cout << "Battery SOC floor: " << BATTERY_RESERVE_FRACTION * 100 << "%\n";
    cout << "Tariff band: INR " << TARIFF_FLOOR << " - " << TARIFF_CEILING
         << " per kWh\n";

    Metrics baselineMetrics, optimizedMetrics;
    vector<HourResult> baselineRows, optimizedRows;
    vector<Battery> batteries;

    for (const auto& c : clusters) batteries.push_back(createBattery(c));

    for (int hour = 0; hour < HOURS; ++hour) {
        for (size_t i = 0; i < clusters.size(); ++i) {
            HourResult b = runBaselineHour(clusters[i], hour);
            HourResult o = runOptimizedHour(clusters[i], batteries[i], hour);

            addToMetrics(baselineMetrics, b, false);
            addToMetrics(optimizedMetrics, o, true);
            baselineRows.push_back(b);
            optimizedRows.push_back(o);
        }
    }

    printMetrics("BASELINE SYSTEM", baselineMetrics);
    printMetrics("GRIDMIND OPTIMIZED SYSTEM", optimizedMetrics);

    cout << "\n========== IMPACT ==========\n";
    double reliabilityGain = optimizedMetrics.reliability()
                           - baselineMetrics.reliability();
    double costSaving = baselineMetrics.totalCost - optimizedMetrics.totalCost;
    double costSavingPct = baselineMetrics.totalCost > 0.0
                         ? 100.0 * costSaving / baselineMetrics.totalCost : 0.0;

    cout << fixed << setprecision(2);
    cout << "Reliability improvement: " << reliabilityGain
         << " percentage points\n";
    cout << "Estimated cost saving: INR " << costSaving << '\n';
    cout << "Estimated cost reduction: " << costSavingPct << "%\n";

    cout << "\n========== BATTERY STATUS ==========\n";
    for (size_t i = 0; i < clusters.size(); ++i) {
        cout << left << setw(16) << clusters[i].name
             << "SOC: " << batteries[i].socMWh << " / "
             << batteries[i].capacityMWh << " MWh"
             << " | SOH estimate: " << batteries[i].stateOfHealth << "%\n";
    }

    writeHourlyCsv(baselineRows, optimizedRows);
    writeKpiCsv(baselineMetrics, optimizedMetrics);

    cout << "\nFiles generated:\n";
    cout << "  outputs/hourly_dispatch.csv\n";
    cout << "  outputs/kpi_comparison.csv\n";
    cout << "\nNote: Results use illustrative synthetic inputs and heuristic rules.\n";
    cout << "They are not validated real-world performance claims.\n";
    return 0;
}
