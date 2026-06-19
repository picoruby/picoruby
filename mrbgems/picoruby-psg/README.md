# picoruby-psg

PSG (Programmable Sound Generator) emulator for PicoRuby.

## Build note for Raspberry Pi Pico

Use the **prod** build when running PSG on R2P2 for Raspberry Pi Pico.
The debug build is too slow for real-time audio playback and can cause severe audio dropouts.

## Wiring (RP2040 and RP2350)

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
 >PWM A───┫ Left  ┣───┫330 Ohm┣───GND
          ┃       ┃   ┗━━━━━━━┛
          ┗━━━━━━━┛
          ┏━━━━━━━┓
          ┃       ┃   ┏━━━━━━━┓
 >PWM B───┫ Right ┣───┫330 Ohm┣───GND
          ┃       ┃   ┗━━━━━━━┛
          ┗━━━━━━━┛
```

For stereo output, you must choose a PWM pair that shares the same slice.
This ensures that both left and right channels are updated simultaneously.

|Slice|PWM A |PWM B |
|-----|------|------|
|PWM0 |GPIO0 |GPIO1 |
|PWM1 |GPIO2 |GPIO3 |
|PWM2 |GPIO4 |GPIO5 |
|PWM3 |GPIO6 |GPIO7 |
|PWM4 |GPIO8 |GPIO9 |
|PWM5 |GPIO10|GPIO11|
|PWM6 |GPIO12|GPIO13|
|PWM7 |GPIO14|GPIO15|
|PWM0 |GPIO16|GPIO17|
|PWM1 |GPIO18|GPIO19|
|PWM2 |GPIO20|GPIO21|
|PWM3 |GPIO22|GPIO23|
|PWM4 |GPIO24|GPIO25|
|PWM5 |GPIO26|GPIO27|
|PWM6 |GPIO28|GPIO29|

## Usage

See README_MML.md
