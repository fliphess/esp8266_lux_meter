# esp8266_light_meter

This is a sketch used with a nodemcu esp8266 with a 128x32 oled display and a bh1750 lux meter sensor.
It displays the last read value on the display and sends the data to a mqtt topic.

I use it in my home assistant setup to determine whether the lights should be turned on on dark days at homecoming.
In a later stadium i'll add my home-assistant configuration as well.

## Pins connecting

### 128x32 OLED display

| Display | ESP8266 |
| ------- | ------- |
| GND     | GND     |
| VCC     | 3v3     |
| SCL     | D1      |
| SDA     | S2      |

### BH1750 Sensor

| Sensor  | ESP8266 |
| ------- | ------- |
| GND     | GND     |
| VCC     | 3v3     |
| SCL     | D1      |
| SDA     | S2      |
| ADDR    | X       |

## TODO

Find a framework for all the boilerplate and focus on the sensor only
