<a href="https://www.buymeacoffee.com/zuidwijk" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>&nbsp;<a href="https://www.zuidwijk.com/product/slimmelezer/" target="_blank">![button_buy-slimmelezer](https://user-images.githubusercontent.com/10123063/127783836-900027f9-e7ea-4084-89e8-89e1cc5f486e.png)</a>
# Please read!!
Since ESPHome version 2021.8.0 the DSMR component is natively in ESPHome. Please use this in your code, as all maintenance and changes are published there and **not** in my code

All documentation is on ESPHome self: https://esphome.io/components/sensor/dsmr.html

A big shout out to @glmnet @klaasnicolaas @frenck and off course @home-assistant & @esphome for improving the code and place it within ESPHome natively!

# DSMR component for ESPHome
The [SlimmeLezer](https://www.zuidwijk.com/product/slimmelezer/) is a compact build easy to use module to read data via the P1 port on a Smart Meter. Based on an ESP8266 (Wemos D1), the SlimmeLezer is perfect to use with [ESPHome](https://esphome.io) and integrates seamless into [Home Assistant](https://www.home-assistant.io).

![IMG_2886](https://user-images.githubusercontent.com/10123063/127781811-f3a67082-32f3-4633-803a-d320bc6af3e4.jpeg)
![IMG_2887](https://user-images.githubusercontent.com/10123063/127781814-8bbe0781-5bdb-4e65-97ac-509afdb0b72d.jpeg)

## DSMR component
The main goal is to create one universal component, which can be used in every country. Though the DSMR (Dutch Smart Meter Requirements) is [specified](https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf) with pre specified OBIS code, not every country has exact the same code. Some examples:

**Version information for P1 output**
- Default OBIS: 1-3:0.2.8.255
- Belgium OBIS: 0-0:96.1.4.255

**Meter Reading electricity delivered to client (Tariff 1) in 0,001 kWh**
- Default OBIS:	1-0:1.8.1.255
- Luxembourg OBIS:	1-0:1.8.0.255

**Meter Reading electricity delivered by dient (Tariff 1) in 0,001 kWh**
- Default	OBIS: 1-0:2.8.1.255
- Luxembourg	OBIS: 1-0:2.8.0.255

Some countries like Luxembourg, Sweden and Hungary, uses kvar next to kW. Therefor all deviant OBIS code is added as extra fields. This gives more sensors than needed, yet it can be used in every country where DSMR based Smart Meters is being used.

### Decryption data for Luxembourg
Smart Meters used in Luxembourg are using encryption. Decryption for Luxembourg is build in the code. This can be defined in the code:
```YAML
dsmr:
  id: dsmr_instance
  decryption_key: '00112233445566778899AABBCCDDEEFF'
 ```
 
 When the key is not set in the code, or when the key changes, it can be set/changes via a Service within Home Assistant, created via below api:
 ```YAML
 # Enable Home Assistant API
api:
  services:
    service: set_dsmr_key
    variables:
      private_key: string
    then:
      - logger.log:
          format: Setting private key %s. Set to empty string to disable
          args: [private_key.c_str()]
      - globals.set:
          id: has_key
          value: !lambda "return private_key.length() == 32;"
      - lambda: |-
          if (private_key.length() == 32)
            private_key.copy(id(stored_decryption_key), 32);
          id(dsmr_instance).set_decryption_key(private_key);
```

In Home Assistant go to Services and select the service ESPHome: {name}_set_dsmr_key. There fill in the code received from the provider:
![SlimmeLezer_set_key](https://user-images.githubusercontent.com/10123063/127783141-52d3ae77-e02b-4296-a1fb-78ab3bbe5ff3.jpg)

 
### Different uart
The SlimmeLezer is built with a logic inverter on the pcb. Connecting that directly to the Rx of the Wemos, causes that it can't be flashed via USB as it constanly pulls the Rx either high or low. Therefor I'm using the 2nd uart, on pin D7. That's why the uart is specified on pin D7 in the code:
```YAML
uart:
  baud_rate: 115200
  rx_pin: D7
```
