# Pico W examples

Board-flavored versions of two top-level demos: real sensors instead of a
sawtooth. Their peers are the top-level observer and central.

## Broadcaster

Reads a thermocouple over SPI, shows the temperature on an I2C LCD, and
broadcasts it.

![Broadcaster](broadcaster_bb.png)

### Pin assign
  * 3V3 to {LCD,THERMO}:VCC
  * GND to {LCD,THERMO}:GND
  * GPIO16 to THERMO:SDO
  * GPIO17 to THERMO:CS
  * GPIO18 to THERMO:SCL
  * GPIO19 to THERMO:SDI
  * GPIO26 to LCD:SDA
  * GPIO27 to LCD:SCL

## Peripheral

Serves the RP2040's internal temperature over GATT. No wiring needed.

## Deploying

`app.rb` goes to `/home/app.rb`; `lcd.rb` and `thermo.rb` go to `/lib/`.
