# ESPHome configuration for Danish energy meters using DSMR
![L+G E360 & Slimmelezer+](https://i.imgur.com/zvcqowP.jpg)

## Description
This is a fork of https://github.com/zuidwijk/dsmr (please read this first), and the configuration (slimmelezer.yaml) is tested on a [Slimmelezer+](https://www.zuidwijk.com/product/slimmelezer-plus/) on a Landis+Gyr E360 energy meter from the grid company "Netselskabet N1"

## How to use
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
This config will give you the following sensors avaliable:
![Sensors](https://i.imgur.com/S4UP0iD.jpg)
