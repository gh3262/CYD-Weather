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
    text_table_page.*   Reusable two-column, eight-row weather page layout
    text_block_page.*   Reusable word-wrapped long-text page layout
  widgets/
    thermometer_widget.* Resizable thermometer graphic
```

`main.cpp` routes each `Screen` enum value through a screen table. To add a
screen, define its draw, enter, and touch-handler functions, add its interface
to `include/app.h`, then add a matching entry to the table. Screens with live
data supply a `poll` function, called every 250 ms while the screen is shown,
that redraws only changed values; `refreshResults` is reserved for one-off
redraws when a background ping or NTP job finishes. Shared device
features are exposed through the CYD and sound helper headers.

Weather pages that use the common two-column layout can render up to eight
rows with `drawTextTablePage()` and route their button touches through
`handleTextTablePageTouch()` from `src/screens/text_table_page.h`. Each row
uses the System screen's 9-point sans-serif font and chooses separate white,
yellow, cyan, or pink label/value colors. The page supplies its background
color, the value-column x position (the label column ends 20 pixels before
it), Back callback, and optional second and third navigation buttons; Home is
provided by the template. The title bar keeps the shared system header color.

Long-text feeds such as `om-current` use `drawTextBlockPage()` and
`handleTextBlockPageTouch()` from `src/screens/text_block_page.h`. The text is
word-wrapped in the same 9-point font into up to eight visible lines, honors
newlines, hard-breaks words too wide for a line, and replaces the unsupported
degree symbol with a space. `firstLine` sets the first wrapped line to show,
and the optional `lineCount` output reports the total, so a page can use its
second and third buttons for Up/Down scrolling, or set `textTapAction` to
page when the text is tapped. A small arrow at the bottom right marks text
continuing below the visible lines. Call
`refreshTextBlockPageText()` to redraw only the text area when a feed changes.

`src/widgets/thermometer_widget.h` provides a resizable thermometer drawing
primitive. Configure its position, size, temperature range, colors, and
optional apparent-temperature fill in `ThermometerWidgetOptions`, then call
`drawThermometerWidget()` to render it. The default size fits a quarter-width
column between the standard header and navigation bar.

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

Adafruit IO's per-minute rate limit applies to data writes, not reads. As a
safety ceiling against runaway polling, the manager still sends at most 60
requests per rolling minute. If Adafruit IO throttles with HTTP 429, all
requests pause for the
`Retry-After` period (60 seconds if none is given), and cached values remain
on screen. A failed fetch is retried after 30 seconds while the feed is
periodic or its page is open.

## Example navigation

| Screen | Button 1 | Button 2 | Button 3 | Button 4 |
|---|---|---|---|---|
| Home | Now | — | Forecast | System |
| Snapshot | Back | Summary | — | Home |
| Summary | Back | Up* | Down* | Home |
| Forecast | Back | Next | Today | Home |
| WiFi Status | Refresh | — | — | Home |
| System | Back | Tools | Refresh | Home |
| Tools | Back | NTP | Cali TS | Home |

Buttons keep a consistent position across screens. Summary shows the
`om-current` feed; its Up/Down buttons (*) appear only when the text needs
more than eight lines. Forecast cycles through `nws.00`–`nws.13`: Next and
Back move forward and backward, wrapping at either end, and Today jumps back
to `nws.00`. Nighttime forecasts (first two words contain "night", any case,
such as "Tonight" or "Tuesday Night") use a
dark gray background; daytime forecasts use dark green. When a forecast is longer than eight lines, a small arrow appears
at the bottom right; tap the text to page down (it wraps to the top). Forecast loads `nws.00`–`nws.04` when opened, then
fetches the next two forecasts in the browsing direction. Button 4 returns Home on
all screens except Home, where it opens System. The System screen also has
WiFi shortcut button in its upper-right corner. Tools has a Ping
button at the upper right and opens touchscreen calibration with **Cali TS**.
