# picoruby-psg

PSG (Programmable Sound Generator) emulator for PicoRuby.

## Wiring

### MCP492x --- 12bit DAC (SPI)

#### Raspberry Pi Pico (2) (W)
```
         ┣
  GPIO19 ┣───COPI>
  GPIO18 ┣───SCK>
     GND ┣───GND
  GPIO17 ┣───CS>
  GPIO16 ┣───LDAC>
 ━━━━━━━━┛
```

The above is an example.
You can choose a GPIO combination as follows:

|COPI  |SCK   |CS |LDAC|
|------|------|---|---|
|GPIO3 |GPIO2 |any|any|
|GPIO7 |GPIO6 |any|any|
|GPIO11|GPIO10|any|any|
|GPIO15|GPIO14|any|any|
|GPIO19|GPIO18|any|any|
|GPIO23|GPIO22|any|any|
|GPIO27|GPIO26|any|any|

#### MCP4921 (monaural)
```
    3.3V                       3.3V
     │    ┏━━━━━━━┓             │
     └────┫1     8┣───OUTmono>  │
    >CS───┫2     7┣───GND       │
   >SCK───┫3     6┣─────────────┘
  >COPI───┫4     5┣───LDAC<
          ┗━━━━━━━┛
```

#### MCP4922 (stereo)
```
    3.3V                       3.3V
     │    ┏━━━━━━━┓             │
     └────┫1    14┣───OUTleft>  │
         x┫2    13┣─────────────┤
    >CS───┫3    12┣───GND       │
   >SCK───┫4    11┣─────────────┤
  >COPI───┫5    10┣───OUTright> │
         x┫6     9┣─────────────┘
         x┫7     8┣───LDAC<
          ┗━━━━━━━┛
```

### Buzzer (PWM)
```
         ┏━━━━━━━┓
         ┃       ┃   ┏━━━━━━━┓
 >PWM1───┫ Left  ┣───┫330 Ohm┣───GND
         ┃       ┃   ┗━━━━━━━┛
         ┗━━━━━━━┛
         ┏━━━━━━━┓
         ┃       ┃   ┏━━━━━━━┓
 >PWM2───┫ Right ┣───┫330 Ohm┣───GND
         ┃       ┃   ┗━━━━━━━┛
         ┗━━━━━━━┛
```

Any GPIO on Raspi Pico can be assigned as PWM1 and PWM2

