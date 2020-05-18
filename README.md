# ESPEASY_Plugin_ITHO_ITHORadio
Alternate version for the ESPEasy ITHO plugin (https://github.com/svollebregt/ESPEASY_Plugin_ITHO) working with a slightly modified version of the ITHORadio library from https://github.com/philipsen/IthoRadio. This library is originally based on the library from: https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho made by 'supersjimmie' and 'klusjesman'.

A CC1101 868Mhz transmitter is needed. The 433Mhz version also seems to work (range may be limited).

For more info see: https://gathering.tweakers.net/forum/list_messages/1690945


# This is a proof of concept and not a final version yet. It is not stable yet and requires some manual configuration. Use at your own risk

## Temporary configuration items

Below will be customizable in the next version of the plugin.

1) Update 'uint8_t _remoteIdRoom[3] = {0x0, 0x0, 0x9};' with your own ID in ithosender.h. See below how to get your remote ID.
2) Update line #135 - #170 of _P145_Itho.ino with the right commands. You can get the commands in a similar way as you get the remote ID (it is part of the serial monitor or log output when you press a button on your original remotes).

## Set-up and configuration
You can use the same set-up and a similar configuration as for the original plugin (source: https://github.com/svollebregt/ESPEASY_Plugin_ITHO):

|CC11xx pin    |ESP pins|Description                                        |
|:-------------|:-------|:--------------------------------------------------|
|1 - VCC       |VCC     |3v3                                                |
|2 - GND       |GND     |Ground                                             |
|3 - MOSI      |13=D7   |Data input to CC11xx                               |  
|4 - SCK       |14=D5   |Clock pin                                          |
|5 - MISO/GDO1 |12=D6   |Data output from CC11xx / serial clock from CC11xx |
|6 - GDO2      |04=D1*  |output as a symbol of receiving or sending data    |
|7 - GDO0      |        |output as a symbol of receiving or sending data    |
| 8 - CSN      |15=D8   |Chip select / (SPI_SS)                             |

*Note: GDO2 is used as interrupt pin for receiving and is configurable in the plugin

Not recommended pins for intterupt:
- Boot pins D3(GPIO0) and D4 (GPIO2) 
- Pin with no interrupt support: D0 (GPIO16)

## List of commands:

1 - set Itho ventilation unit to low speed

2 - set Itho ventilation unit to medium speed (auto1)

3 - set Itho ventilation unit to high speed

13 - set itho to high speed with hardware timer (10 min)

23 - set itho to high speed with hardware timer (20 min)

33 - set itho to high speed with hardware timer (30 min)

## Not implemented commands:
These commands are not (yet) implemented.

4 - set Itho ventilation unit to full speed

1111 to join ESP8266 with Itho ventilation unit

9999 to leaveESP8266 with Itho ventilation unit

0 - set Itho ventilation unit to standby

## List of States:

1 - Itho ventilation unit to lowest speed

2 - Itho ventilation unit to medium speed

3 - Itho ventilation unit to high speed

4 - Itho ventilation unit to full speed

13 -Itho to high speed with hardware timer (10 min)

23 -Itho to high speed with hardware timer (20 min)

33 -Itho to high speed with hardware timer (30 min)

### Get your remote IDs
In the plugin you are able to define 3 RF device ID's for the existing RF remote controls the plugin is listening to, to update the state of the fan.

You are able to capture the id of you RF remote, by setting the log settings to 3, in the advanced settings menu. After pressing a button, you will see the ID (8 chars) of the RF in the log. Use ID with ':'. You can also use serial monitor for this.
#### example ID: xx:xx:xx

As in the original plugin, in case a timerfunction is called (timer 1..3), an internal timer is running as estimate for the elapsed time.

The plugin will publish MQTT topics as they change. The aquisition cycle time should be used as a state update cycle time.
In case a topic doesnT change the cycle time is used for cyclic update. It is recommended to set this to higher values: for example to 60s

When using a Wemos D1 mini, you have to remove D1 and D2 from I2C on the hardware page, because one of the pins must be used as Intterupt pin. See note above concerning the interrupt pins.

### Paring the ESP8266 remote with the fan

With this plugin, to control the fan, the ESP8266 does not have to be "paired" with the fan. It is assumed that you pair your existing physical remote with the fan and use the ID of that physical remote in this plugin. Join and leave functions have therefore been disabled by default in the code. If you enable it you can use the browser (http://YourIP-adress/control?cmd=STATE,1111) or mosquitto command line to pair (mosquitto_pub -t /Fan/state/cmd -m 'state 1111')
