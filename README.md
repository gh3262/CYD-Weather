# CYD Weather Display based on CYD-Framework Project

The goal of this project is to collect various weather feeds from AdafruitIO and
display different useful weather data such as current weather, forecasts, etc.

A PlatformIO foundation for projects built on the ESP32 Cheap Yellow Display
(CYD, ESP32-2432S028R). It provides the display, touch, navigation, sound,
WiFi connection, and clock building blocks so a project can add
its own screens and behavior without rebuilding the device layer.

## Included framework features

- **Display and navigation:** shared CYD initialization, theme, title bar,
  navigation buttons, touch mapping, and button hit-testing.
- **WiFi connection:** loads configured networks from `credentials.h`, scans
  for them during connection/retry, and connects to the strongest known
  network. WiFi status is available on the status screen.
- **Clock:** NTP synchronization at startup and on demand, local time zone
  configuration, title-bar clock, and time/date formatting helpers.
- **Touch calibration:** a four-target calibration screen accessible from
  Tools; successful raw touch limits are saved in ESP32 non-volatile storage.
- **Sound:** non-blocking built-in tones and tone sequences for the speaker
  amplifier.
- **Example screens:** Home, WiFi Status, System, and Tools demonstrate
  how to compose screens with the shared navigation layout.

The WiFi connection code scans for configured access points internally. There
is no separate nearby-network scanner page.

## Project layout

```text
include/
  app.h                 Application state and screen interfaces
  cyd.h                 CYD helper umbrella header
  cyd_pins.h            Board pin and touch calibration definitions
  cyd_theme.h           Colors, fonts, dimensions, and navigation layout
  cyd_display.h         Display, title bar, clock, and button helpers
  cyd_touch.h           Touch initialization and navigation hit tests
  cyd_time.h            NTP synchronization and time/date helpers
  cyd_helpers.h         LED, backlight, light sensor, and uptime helpers
  credentials*.h        WiFi/NTP settings template and local configuration
src/
  main.cpp              Startup, WiFi connect/maintain, screen table, touch loop
  cyd_globals.cpp       Shared display and touch objects
  adafruitio/
    adafruitio_manager.* Active-page polling and cached values
    feed_catalog.csv     Editable feed catalog
    feed_catalog.cpp     Compiled feed definitions
  audio/
    sound_manager.*     Non-blocking sound playback
  screens/              Example screens
```

`main.cpp` routes each `Screen` enum value through a screen table. To add a
screen, define its draw, enter, and touch-handler functions, add its interface
to `include/app.h`, then add a matching entry to the table. Shared device
features are exposed through the CYD and sound helper headers.

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy `include/credentials_template.h` to `include/credentials.h` (the
   configuration file is gitignored) and set:
   - One or more WiFi SSID/password pairs in `wifiNetworks`.
   - `NTP_SERVER_1` and `NTP_SERVER_2`.
   - `TZ_STRING` to a POSIX time zone, such as `CST6CDT,M3.2.0,M11.1.0`.
   - `AIO_USERNAME` and `AIO_KEY` for Adafruit IO.
3. Build and upload with `pio run -t upload`; open the serial monitor at
   115200 baud.

Library and platform versions are pinned in `platformio.ini`.

## Adafruit IO feeds

`src/adafruitio/feed_catalog.csv` is an editable export of the feed catalog.
The firmware currently compiles definitions from `feed_catalog.cpp`; CSV edits
must also be applied there to affect the build. `src/adafruitio/adafruitio_manager.*`
handles active-page polling. Use `adafruitIOManager` to call
`setActiveFeeds()` when a page is entered, then read cached values with
`getFeedValue()` and update times with `getFeedUpdatedAt()`. Values are fetched
over the HTTPS API and kept as `String`s so long alert text is not truncated.
Each catalog entry also indicates whether its value may exceed 45 characters,
so page layouts can plan for wrapping or scrolling. The polling task runs
independently of the display loop. `Periodic` feeds are refreshed every minute
even when their page is not active; `OnDemand` feeds are fetched when selected
and retain their cached value until selected again.

## Example navigation

| Screen | Button 1 | Button 2 | Button 3 | Button 4 |
|---|---|---|---|---|
| Home | — | — | — | System |
| WiFi Status | Refresh | — | — | Home |
| System | Back | Tools | Refresh | Home |
| Tools | Back | NTP | Cali TS | Home |

Buttons keep a consistent position across screens. Button 4 returns Home on
all screens except Home, where it opens System. The System screen also has
WiFi shortcut button in its upper-right corner. Tools has a Ping
button at the upper right and opens touchscreen calibration with **Cali TS**.
