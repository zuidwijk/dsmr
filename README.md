# ESPHome configuration for Danish (and possibly other) energy meters using DSMR on P1 port
![L+G E360 & Slimmelezer+](https://i.imgur.com/zvcqowP.jpg)

## Description
This is a fork of https://github.com/zuidwijk/dsmr (please read this first).

The configuration (slimmelezer.yaml) is tested and confirmed working on a [Slimmelezer+](https://www.zuidwijk.com/product/slimmelezer-plus/) on a Landis+Gyr E360 energy meter from the Danish grid companies:
- Netselskabet N1
- Vores Elnet
- Please let me know if you can confirm the configuration works on other grid companies as well, so I can update the list.

## How to use
- Make sure you have the Home Assistant Add-On ESPHome installed and updated to latest version. More info: https://esphome.io/guides/getting_started_hassio.html
- This configuration is only relevant for those countries where the OBIS values 1.8.0 and 2.8.0 etc are in use. If your utility company uses OBIS 1.8.1. 1.8.2. 2.8.1 and 2.8.2 I recommend that you use the configuration from the https://github.com/zuidwijk/dsmr repo. 
- First off, contact your utility company (Netselskab) to have the P1 port enabled, and await their confirmation.
- Regarding Landis+Gyr E360, certain Energy Meter serial numbers (56xxxxx + 57xxxxx) have the wrong firmware, hence P1 port cannot be activated. N1 expects to have new firmware in Q1 2023. Energy Meters with serial number 58xxxxx and 59xxxxx can be activated right away.
- The sensors `sensor.energy_exported` and `sensor.energy_imported` can be used in the Home Assistant Energy Dashboard
- (only relevant if you produce energy (solar/wind etc.)) The DSMR protocol does not provide a sensor for actual power with negative values for export and positive values for import. Use the following template sensor to create one:
```
- sensor:
    - name: Grid Active Power
      unique_id: grid_active_power
      unit_of_measurement: "W"
      device_class: power
      state: >-
        {% set power_import = states('sensor.power_import') | float %}
        {% set power_export = states('sensor.power_export') | float %}
        {{ ((power_import - power_export) * 1000) | round }}
```

## Sensors
This config will give you the following sensors avaliable (example from Home Assistant):
![Sensors](https://i.imgur.com/S4UP0iD.jpg)
