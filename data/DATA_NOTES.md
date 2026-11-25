# Data notes

No operator or historical dataset is bundled with this prototype.

The current simulator generates its 24-hour demand, renewable availability and market-price profiles in `src/main.cpp`. These values are illustrative scenario assumptions.

When real data becomes available, recommended hourly input columns are:

- timestamp
- cluster
- ev_energy_demand_mwh
- renewable_available_mwh
- iex_price_inr_per_kwh
- battery_soc_mwh
- battery_soh_percent
- grid_import_limit_mw

Keep timestamps timezone-consistent (preferably IST for the Indian operating scenario), document missing-value handling, and retain a separate untouched test period for validation.
