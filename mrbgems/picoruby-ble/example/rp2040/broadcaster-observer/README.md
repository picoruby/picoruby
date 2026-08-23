## Example of BLE (Broadcaster - Observer pattern)

## Broadcaster

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

### Source

```console
/home/app.rb
/lib/lcd.rb
/lib/thermo.rb
```

## Observer

![Observer](observer_bb.png)

### Pin assign
  * 3V3 to USB-TTL:VCC
  * GND to USB-TTL:GND
  * GPIO16 to USB-TTL:RX
  * GPIO17 to USB-TTL:TX

### Source

```console
/home/app.rb
/lib/led.rb
```
