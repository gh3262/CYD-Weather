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
- **Backlight timeout:** GPIO 21 starts HIGH (backlight on). After five
  minutes without a touch, it goes LOW to turn only the backlight off.
  Touch anywhere to wake it and restart the timer; the wake press does not
  activate buttons or beep, and must be released before another action.
  The timer starts after startup finishes, and a held touch keeps the light on.
  The processor, WiFi, feed polling, and screen updates continue normally;
  this is not processor sleep or display-controller power-down.
- **WiFi connection:** loads configured networks from `credentials.h`, scans
  for them during connection/retry, and connects to the strongest known
  network. WiFi status is available on the status screen.
- **Clock:** NTP synchronization at startup and on demand, local time zone
  configuration, title-bar clock, and time/date formatting helpers.
- **Touch calibration:** a four-target calibration screen accessible from
  Tools; successful raw touch limits are saved in ESP32 non-volatile storage.
- **Sound:** non-blocking built-in tones and tone sequences for the speaker
  amplifier. Every active button plays a short 30 ms, 2.5 kHz beep when a
  press is detected: hit-test buttons with `buttonPressed()` (the `navPressed*`
  helpers already do) instead of `touchInRect()`, after checking that the
  button is active, so hidden or placeholder buttons stay silent. The beep
  blocks briefly so a following screen redraw cannot lengthen it.
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
    adafruitio_manager.* Background worker: feed polling, value cache, history
    feed_catalog.csv     Editable feed catalog
    feed_catalog.cpp     Compiled feed definitions
    numeric_history.*    Timestamp parsing, bucketing, and aggregation
    numeric_history_page.h  Shared JSON page parser (firmware and tests)
  audio/
    sound_manager.*     Non-blocking sound playback
  screens/
    Shared page building blocks:
    text_table_page.*   Reusable two-column, eight-row weather page layout
    text_block_page.*   Reusable word-wrapped long-text page layout
    history_chart_page.*  Reusable timestamped line/daily-bar history page
    Pure logic (no display code, native-tested):
    weather_descriptor.h  Pressure-based one-word forecast (Home)
    wind_direction_stats.*  Speed-weighted mean, range, steadiness, wind rose
    wind_direction_snapshot.h  Precomputed wind graphic JSON contract
    Weather screens:
    home_screen.cpp     Clock, temperature, weather word, navigation
    snapshot_screen.cpp, summary_screen.cpp, forecast_screen.cpp
    details_screen.cpp  Hub for the detail pages below
    temperatures_screen.cpp, temperature_history_screen.cpp, temperature_daily_screen.cpp
    barometer_screen.cpp, barometer_history_screen.cpp
    precipitation_screen.cpp, precipitation_history_screen.cpp
    wind_screen.cpp     Wind table; Dir and Speed buttons
    wind_direction_screen.cpp  12-hour compass dial and wind rose
    wind_history_screen.cpp    72-hour speed/gust chart
    clouds_screen.cpp, clouds_history.h  Daily cloud ranges and sunshine forecast
    snapshot_history_page.*   Shared combined-feed history loading/refresh
    astronomical_screen.cpp, alerts_screen.cpp (list, details, full text)
    System screens:
    system_screen.cpp, wifi_status_screen.cpp, tools_screen.cpp,
    touch_calibration_screen.cpp
  widgets/
    thermometer_widget.* Resizable thermometer graphic
    line_chart_widget.*  Line, multi-series, and bar charts with Y scale
test/
  README                Native test build/run commands
  line_chart_widget/    Chart drawing against recording graphics stubs
  numeric_history/      History parsing/aggregation and manager with fake HTTP
  weather_descriptor/   Compile-time static_assert checks
  wind_direction_stats/ Wind direction statistics
  wind_direction_snapshot/ Wind graphic feed/parser/renderer integration
```

`main.cpp` routes each `Screen` enum value through a screen table. To add a
screen, define its draw, enter, and touch-handler functions, add its interface
to `include/app.h`, then add a matching entry to the table. Screens with live
data supply a `poll` function. Every 250 ms the router checks a thread-safe
feed-cache revision; it calls the visible screen's poll only after cached data
or history availability changes, or after a page/background-job redraw.
Unchanged fetches and failed requests do not trigger repeated formatting.
Screens still compare displayed values before drawing; `refreshResults` is reserved for one-off
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
`drawTextTablePage()` resets the renderer's cache on entry.
`refreshTextTablePageRows()` leaves unchanged rows alone and clears only a
changed value's row area in the value column, preserving its label. Label or
label-color changes redraw that entire row. Layout/background changes clear
and redraw the content area, including spacer rows.

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
The renderer caches the most recent source text, one packed buffer of wrapped
text, and line offsets/lengths. Scrolling and repeated draws of unchanged text
reuse this layout without sanitizing, measuring, or allocating per-line strings.
Changing text rebuilds the cache; blank paragraphs, long-word splitting, line
counts, and overflow indicators retain the same behavior.

`src/widgets/thermometer_widget.h` provides a resizable thermometer drawing
primitive. Configure its position, size, temperature range, colors, and
optional apparent-temperature fill in `ThermometerWidgetOptions`, then call
`drawThermometerWidget()` to render it. The default size fits a quarter-width
column between the standard header and navigation bar.

`src/widgets/line_chart_widget.h` provides a feed-independent line chart
(one or more series). Set the position, size, `minimumX`/`maximumX`, and
`minimumY`/`maximumY` in `LineChartWidgetOptions`, then pass an array of
`LineChartPoint` values to `drawLineChartWidget()`. Points must be ordered by
nondecreasing X (duplicate X values are allowed). Use `NAN` for a missing Y
reading to leave a gap rather than draw a zero. Empty data draws just the
chart background/grid/axes; a single in-range point is visible as a pixel.

The widget clips line segments at all four plot edges, preserves their slopes,
and never draws outside its rectangle. Axes, quarter-interval grid lines, point
markers, and colors are configurable. It redraws its rectangle on every call;
the calling page should redraw only when its data or options change. Invalid
options or data are logged and return `false` before any drawing. It does not
retain points, allocate a history buffer, or fetch feeds.
Enable `showYScale` to reserve a left gutter for numeric Y labels and ticks.
`yTickInterval` and `yLabelInterval` default to 5 and 10, respectively;
both are anchored at multiples of their interval, including negative values.
`yLabelDecimalPlaces` supports 0-3 decimal places and `labelColor` sets the
label color. Invalid, overly dense, or non-fitting scales are logged and
rejected before drawing. The scale uses the built-in font and restores the
standard default font, text size 2, and text color afterward; with the scale
disabled the widget does not change text settings. Titles and X labels remain
the containing page's responsibility.

```cpp
#include "widgets/line_chart_widget.h"

LineChartWidgetOptions chart;
chart.x = 10;
chart.y = 45;
chart.width = 300;
chart.height = 140;
chart.minimumX = 0;
chart.maximumX = 4;
chart.minimumY = 40;
chart.maximumY = 90;
const LineChartPoint points[] = {
    {0, 61}, {1, 63}, {2, 58}, {3, 66}, {4, 69},
};
if (!drawLineChartWidget(chart, points, sizeof(points) / sizeof(points[0])))
    Serial.println("Could not draw chart.");
```

X coordinates are `double` so Unix timestamps retain their resolution.
Callers can instead use sample positions `0` through `count - 1`, but must not
label these as hourly data. Timestamped retrieval and aggregation live in the
Adafruit IO manager, outside the widget; the Temperature, Barometer,
Precipitation, and Wind History pages and Wind Direction use that path.

To plot several lines on shared axes, pass an array of `LineChartSeries`
(`points`, `pointCount`, `color`) to `drawLineChartSeriesWidget()`. Series are
drawn in array order, each in its own color. If any series is invalid, the
call is rejected before anything is drawn. `drawLineChartWidget()` is the
single-series form and uses `options.lineColor`.

`drawBarChartWidget()` shares the same options and axes, using each point's X
as a bar center and a caller-supplied width in X units. The Y axis must start
at zero; values must be nonnegative (NaN omits a bar). Bars are clipped to the
plot, zero-height bars stay empty by default, and positive amounts smaller than
one pixel are drawn one pixel high. An optional `zeroBarHeight` draws zero values
as fixed-height markers in `zeroBarColor`, without changing their numeric values.

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

`src/adafruitio/feed_catalog.csv` is the editable feed catalog. Its
`refresh_policy` column is `periodic`, `page_periodic`, or `on_demand`; periodic feeds specify
their cadence in `refresh_interval_minutes`, while on-demand feeds leave that
field blank. Keep the compiled definitions in `feed_catalog.cpp` synchronized
with catalog edits. `src/adafruitio/adafruitio_manager.*` handles active-page
polling. Use `adafruitIOManager` to call
`setActiveFeeds()` when a page is entered, then read cached values with
`getFeedValue()` and update times with `getFeedUpdatedAt()`. Values are fetched
over the HTTPS API and kept as `String`s so long alert text is not truncated.
Each catalog entry also indicates whether its value may exceed 45 characters,
so page layouts can plan for wrapping or scrolling. The polling task runs
independently of the display loop. Periodic feeds are refreshed at their
catalog interval even when their page is not active; on-demand feeds are
fetched when selected and retain their cached value until selected again.
Combined `pages.*` feeds use `page_periodic`: fetch immediately on entry and
at their catalog interval only while selected (one minute, except Clouds at
one hour). Their cached JSON is released on departure,
and in-flight responses for a departed page are discarded. This avoids keeping
all large history snapshots in RAM or polling them while other pages are open.

For feeds with separate entries, `setFeedHistoryCount()` requests the latest
1-1000 entries and `getFeedValues()` reads them in newest-first order. History
uses the same background worker, request budget, and retry handling as single
values. Only the JSON value fields are retained, including full long text.
`getFeedHistoryEntry()` copies just the selected entry and returns the
available count under the same cache lock. Alert Details uses this instead
of copying the whole history on each refresh. An out-of-range selection
resets to the first entry; pending and empty histories remain distinct.

### Timestamped numeric history

`requestNumericHistory(feedKey, request)` starts one chart-history subscription,
independent of the latest-value cache and value-only alert history. The feed
must have a numeric catalog type. `NumericHistoryRequest` defaults to a rolling
86,400-second window, 3,600-second buckets, and `HistoryAggregation::Average`.
It also supports Minimum, Maximum, Latest, and CircularAverage (numeric bearings
in degrees, such as `currentwinddegrees`). Circular averages with cancelling
directions are undefined rather than an invented bearing.

Set `request.window` to `HistoryWindow::CompletedUtcDays` for whole recorded
dates ending at the device's local today's date encoded as UTC midnight,
excluding the local current date. This mode requires
86,400-second buckets and a whole number of days; the default remains Rolling.
Date-only totals stamped at midnight retain their recorded date rather than
being shifted into the previous date by local-time conversion.

`getNumericHistory(result, status)` returns the last complete result and a
Loading/Ready/Failed/Idle status. A successful empty window is different from a
failed retrieval. Refresh failures preserve the last complete result; partial
pages are never published. `cancelNumericHistory()` cancels further retrieval
and releases the completed cache; an in-flight response is discarded.

`requestNumericHistory(feedKeys, requests, count)` subscribes to up to
`MAX_NUMERIC_HISTORY_SERIES` (2) numeric feeds at once. All requests must use
the same window, alignment mode, and bucket size, but each can have its own aggregation. Series
are fetched one after another against the same fixed UTC window, and the
results are published together only after every series completes. A failure
keeps every previous result. `getNumericHistory(results, count, status)` reads
the results in request order; the single-result form returns the first series.

The existing background worker fetches `id`, `value`, and `created_at` in pages
of at most 100 records, filtering irrelevant JSON fields. Both UTC time filters
are fixed for the entire pagination run. The start filter includes a one-second
overlap, then local filtering enforces `[startUtc, endUtc)`. The worker extracts
only the opaque `before` cursor from the API's documented Link header, builds
subsequent requests against the same feed, and shares the existing rolling
request budget, TLS verification, HTTP 429 pause, and retry handling. Ordinary
feed polling runs between pages. It refreshes a completed chart window after
60 seconds and retries failed windows after 30 seconds, while subscribed.

`created_at` is parsed as UTC, with optional fractional seconds. Buckets are
anchored to Unix-epoch multiples of the requested interval, not the first
reading. Partial boundary buckets average their available readings normally.
Only IDs repeated in overlapping pages are deduplicated; distinct records with
equal values count normally. Each bucket retains count, sum, min, max, and latest
value/time (plus sine/cosine sums for circular mode). The result also retains raw
observed extrema and invalid-value counts. Empty buckets return NaN, preserving
gaps. Invalid numeric values are counted and logged; missing/malformed timestamps,
record IDs, JSON, or pagination cause a failed window, not apparent data loss.

Memory grows with bucket count and one bounded response page, not total sample
count. Limits are 512 buckets, a 31-day window, 256 pages per refresh, and 1,000
distinct records at one frontier millisecond for overlap deduplication. Exceeding
a retrieval limit is an explicit failure, never a silently truncated chart.
An unset system clock also fails rather than inventing a time window.
No SD-card storage is used yet; retrieval/aggregation and drawing are separate
so persistent storage can be added later. Local timezone conversion is for
labels only; UTC bucket identities remain distinct across DST changes.

`getFeedUpdatedAt()` remains the existing cache-fetch `millis()` value; it is
not the observation's `created_at` timestamp.

`Time` feeds (such as `days1sunrise`) hold UTC timestamps like
`2026-10-20T20:21:25Z`. `formatUtcTimestampLocal()` in `cyd_time.h` converts
them to local time using `TZ_STRING`, including daylight saving time, with an
optional `strftime` format (default `%I:%M %p`). It returns `--` if the value
can't be parsed.

`days1moonphase` arrives as camelCase (for example `waxingGibbous`).
`moonPhaseDisplayName()` in `cyd_helpers.h` converts it to display text such
as `Waxing Gibbous`, `Full Moon`, or `New Moon`.

Adafruit IO's per-minute rate limit applies to data writes, not reads. As a
safety ceiling against runaway polling, the manager still sends at most 60
requests per rolling minute. If Adafruit IO throttles with HTTP 429, all
requests pause for the
`Retry-After` period (60 seconds if none is given), and cached values remain
on screen. A failed fetch is retried after 30 seconds while the feed is
periodic or its page is open.

A red dot to the left of the header clock marks active weather alerts. It
follows the periodic `nbr-alerts` feed: the dot shows when the count is above
zero and is hidden (header color) when the count is zero or not yet loaded.
While alerts are active, Home also shows a red "Active Alert" button between
the temperature and the navigation bar that opens the Alerts page.

Without an active alert, Home uses that area for a single pressure-based
weather word. The trend is the 24-hour forecast (`hours24pressure`) minus
current pressure (`currentpressure`), both in hPa. Rules are checked in order:

| Condition | Word |
|---|---|
| Pressure < 995 and trend < -2 | Stormy |
| Pressure < 1005 and trend > +2 | Clearing |
| Trend > +3 | Improving |
| Trend < -3 | Unsettled |
| Pressure > 1025 | Fair |
| Otherwise | Nice |

Current pressure refreshes every minute and the 24-hour forecast every 30
minutes. Missing, nonnumeric, nonfinite,
or nonpositive pressure shows `--`; invalid values are logged. This is a
simple pressure outlook, not a description of current rain or cloud cover.
Active alerts always replace the word with the existing tappable alert button.

Alerts shows the latest `headline-alert` paragraph followed by the active count
(for example, " - 2 Alerts"). Details fetches the newest `nbr-alerts` entries
from `weather-alerts`, one per page, newest first. Back and Next wrap through
the available entries; Alerts returns to the headline page. If fewer entries
are available than the reported count, the page notes this rather than making
up missing entries. Tap headline or detail text to page through long entries,
wrapping back to the beginning.

Full Alert shows the latest `big-alert` value without truncation, with Up and
Down paging through its wrapped text and Back returning to the headline.
Each alert view fetches its text when entered; a changed alert count also
requests fresh text and resets scrolling. When the count is zero or unavailable,
all three views show "No Current Alerts" instead of old cached alert text.
The API allows at most 1000 history entries in a request; a larger alert count
produces a logged error and an explanatory Details message, not a truncated list.

## Example navigation

| Screen | Button 1 | Button 2 | Button 3 | Button 4 |
|---|---|---|---|---|
| Home | Now | Forecast | Details | System |
| Details | Back | — | — | Home |
| Precipitation | Back | — | History | Home |
| Precipitation History | Back | — | — | Home |
| Barometer | Back | — | History | Home |
| Barometer History | Back | — | — | Home |
| Wind | Back | Dir | Speed | Home |
| Wind Direction | Back | — | Speed | Home |
| Wind History | Back | Dir | — | Home |
| Clouds | Back | — | — | Home |
| Temperatures | Back | Daily | History | Home |
| Temperature History (72 hours) | Back | Daily | — | Home |
| Daily Temperatures (30 days) | Back | — | History | Home |
| Astronomical | Back | — | — | Home |
| Snapshot | Back | Summary | — | Home |
| Summary | Back | More/Up* | Forecast | Home |
| Forecast | Back | Next | Today | Home |
| Alerts | Back | Details | Full Alert | Home |
| Alert Details | Back | Next | Alerts | Home |
| Full Alert | Back | Up | Down | Home |
| WiFi Status | Back | Refresh | — | Home |
| System | Back | Tools | Beeps/Silent | Home |
| Tools | Back | NTP | Cali TS | Home |

Buttons keep a consistent position across screens. Summary shows the
`om-current` feed; its More button (*) appears only when the text needs
more than eight lines. More shows the second page and changes to Up, which
returns to the top. Summary's Forecast button opens the same 14 forecast
pages as Home's Forecast button. Forecast cycles through `nws.00`–`nws.13`: Next and
Back move forward and backward, wrapping at either end, and Today jumps back
to `nws.00`. Nighttime forecasts (first two words contain "night", any case,
such as "Tonight" or "Tuesday Night") use a
dark gray background; daytime forecasts use olive green. When a forecast is longer than eight lines, a small arrow appears
at the bottom right; tap the text to page down (it wraps to the top). Forecast loads `nws.00`–`nws.04` when opened, then
fetches the next two forecasts in the browsing direction. Button 4 returns Home on
all screens except Home, where it opens System. The System screen also has
WiFi shortcut button in its upper-right corner, with a Refresh button below it.
System's **Beeps** button toggles button-press beeps; while muted it is gray
and reads **Silent**. Beeps are on at startup and the setting is not saved.
Muting affects only button beeps, not other sounds. Tools has a Ping
button at the upper right and opens touchscreen calibration with **Cali TS**.

System shows **Largest Block**, the largest currently available contiguous
internal 8-bit heap allocation in KB, and **AIO Stack Free**, the minimum
unused feed-worker stack since startup in bytes (ESP32 FreeRTOS units).
The stack field shows **Not running** if the worker has not started.
These diagnostics, together with free/minimum heap, are sampled when System
is opened or its Refresh button is pressed; they do not poll the network.

Details provides a two-column, four-row button grid in the left two-thirds
of the content area. The left column contains **Temps**, **Baro**, **Precip**,
and **Astro**, opening Temperatures, Barometer, Precipitation, and Astronomical,
respectively. The top-right **Wind** button opens Wind; **Clouds** directly
below it opens Clouds. The remaining two slots
are inactive placeholders. The right third shows the
thermometer using `feed-avg` in red and `currenttemperatureapparent` in cyan,
on a 10 to 100 F scale. The tube is centered in its panel, with Fahrenheit
ticks on the left and Celsius ticks on the right (enabled by
`ThermometerWidgetOptions::showCelsiusScale`). Until a temperature is available it shows `-- F`
instead of a default reading.

**Clouds** reads only the latest `pages.om-sun` snapshot, fetched on entry and
refreshed hourly while open. The producer updates the feed every six hours.
The feed supplies 28 daily records covering 14 past days and
14 forecast days:

```json
[{"ts":"20261111","dld":10.43,"ssd":8.0,"pct_ss":76.72,"cc_mean":22.62,"cc_max":80.0,"cc_min":0.0}]
```

`ts` is a local calendar date (`YYYYMMDD`). On a fixed 0-100% scale, yellow
bars rise from zero to the supplied `pct_ss` (sunshine as a percentage of
daylight hours) and occupy the entire daily slot, with no gaps between adjacent
nonzero dates. A red line overlays `cc_mean`, the supplied average daily cloud
cover percentage. The two series are independent; neither is calculated from
the other. `dld`, `ssd`, `cc_min`, and `cc_max` are ignored.
All supplied dates are shown, including forecasts, with actual first/last date
labels and a day count. The display does not sort, aggregate, or fill dates.
A dashed white vertical divider marks the start of today in local time,
with **Past** on its left and **Forecast** on its right. Today is included on
the forecast side. The divider follows the date, not the midpoint, and moves
at local midnight even if the feed has not changed. Its labels occupy a row
above the data; a label is omitted if its side is too narrow to fit. No divider
is drawn when today falls outside the supplied window or the clock is unavailable.
Null sunshine omits its bar, and null mean cloud cover breaks the line.
Zero sunshine has no bar height; zero cloud cover remains a real line reading.

Invalid dates, missing/non-numeric plotted fields, percentages outside 0-100,
unordered/duplicate dates, or snapshots exceeding 72 records are
logged and rejected atomically, retaining any previous data as stale.
An empty array shows no readings. Loading, memory-failure, retry, and offline
behavior use the shared snapshot-history page. Staleness uses feed receipt age
(two hours, or two polling intervals), not forecast dates. No raw numeric history is requested.
Back returns to Details; Home returns Home.

Barometer uses a monospaced three-column table with cyan **Time**, **Pressure**,
and **Trend** headers. Its rows show Current, 1 Hour, 2 Hours, 6 Hours, 12 Hours,
and 24 Hours using `pages.p04-baro`: six pressure/trend pairs in that order.
Pressure is in hPa;
unavailable values show `--`. Only changed rows are redrawn as feeds arrive.
Back returns to Details.

Precipitation uses only `pages.p02-precipitation`: 21 row-major chance/type/amount
values for 1 hour, 2 hours, 6 hours, 12 hours, 1 day, 2 days, and 5 days.
The former Precip 2 test page is now the main page; its duplicate button and
route have been removed. Chance retains its percent suffix, type its six-character
display limit, and amount its inches formatting. Malformed snapshots are logged
and show `--` across the table.

Precipitation has a **History** button in navigation position 3. It opens
**Precip - 30 Days**, a cyan daily bar chart of `pages.prcp-line` in inches.
The feed contains `[{"ts":"20261104","prcp":0.12}, ...]`, with local calendar
dates (`YYYYMMDD`) and one daily total per date. The window covers 30 calendar
dates ending at the newest supplied date (normally yesterday), without converting
date labels through the local timezone. Older entries are discarded.
The supplied dates and totals are plotted directly. Dry dates
have a yellow 3-pixel bar so all 30 days can be counted; precipitation
bars remain cyan. Bars occupy 75% of each daily slot, leaving a small gap.
These visual zero markers do not contribute to the total or peak.
The Y axis starts at zero,
uses an automatic fractional-inch scale, and keeps small positive amounts
visible. The summary shows the sum of the 30 daily totals and the largest daily
amount, to three decimal places. Bottom labels show the first and last recorded
dates without local-time conversion.

Retrieval uses only the latest combined snapshot, not raw-record pagination.
The full snapshot is validated before displaying any result. A successfully empty
array contains no plotted daily readings;
loading and failed requests remain distinct. Refresh failures or WiFi loss mark
retained data **Stale**; crossing local midnight also marks yesterday's window
stale until the new window loads. Back returns to Precipitation; Home returns
Home. Leaving deactivates snapshot polling when the next page selects its feeds.

Temperatures has a **Daily** button in navigation position 2, alongside the
existing **History** button in position 3. Daily opens **Daily Temps - 28 Days**,
a floating bar chart using only the latest `pages.om-temp` snapshot.
Past bars are cyan, today's bar is yellow, and future bars are green.
Colors follow the local date at midnight along with the divider; if the clock
is unavailable, all bars remain cyan.
The feed contains `[{"ts":"20261111","t2m_min":47.42,"t2m_max":75.06,"at_min":45.04,"at_max":72.36}, ...]`,
with local `YYYYMMDD` dates and daily minimum/maximum Fahrenheit temperatures.
It shows all 28 supplied dates: 14 past days and 14 forecast days, including
today on the forecast side. Date-only labels are encoded as UTC midnight,
with actual first/last date labels and no leading empty window. Values are already
aggregated and ordered earliest to latest by the producer; the
display does not aggregate, sort, or fill them. Apparent temperatures
(`at_min` and `at_max`) are ignored. Supplied zeros are plotted as
real values. Each bar spans `t2m_min` to `t2m_max`, with 75% slot width and a
gap between dates. Negative temperatures
are supported; equal high/low values draw a one-pixel bar. Dates missing either
endpoint via explicit null are left blank, not treated as zero. Invalid dates,
missing/non-numeric fields, inverted ranges, unordered/duplicate dates, or snapshots
exceeding 72 records are logged and rejected atomically, retaining prior data as stale.
The summary shows the lowest low and highest high among paired
dates, and the footer counts paired dates. Both series are decoded atomically
from the same snapshot, with the shared loading, empty, stale, and cancellation
behavior. The feed is fetched on entry and refreshes hourly while active; no individual
temperature history feeds are requested. Staleness uses feed receipt age
(two hours, or two polling intervals), not forecast dates. Like Clouds, a dashed white vertical line
marks the start of today with **Past** on the left and **Forecast** on the right.
The divider moves at local midnight without requiring a feed change; it is
omitted outside the supplied window or when the clock is unavailable.
Labels sit above the data and are omitted on sides too narrow to fit.
Back returns to Temperatures, Home returns Home, and **History** in position 3
opens Temperature History. Temperature History has **Daily** in position 2
to switch back. Sunshine and daylight feeds are not used by this page.

Barometer has a **History** button in navigation position 3. It opens
**Last 72 Hours**, a `pages.baro-line` chart that uses the same shared history
page as Temperature History (`screens/history_chart_page`), with the same
loading, empty, stale, and cancellation behavior. The snapshot contains
`[{"ts":"2026110513","baro":1013.09}, ...]`: local calendar hours and hourly
average hPa. It shows the newest 72 elapsed hours using the supplied values
without averaging or gap filling. The Y range is the observed
minimum minus 2 hPa through the observed maximum plus 2 hPa; label spacing
(1, 2, 5, 10, 20, or 50 hPa) is the finest that gives at most eight labels.
Back returns to Barometer; Home returns Home.

Barometer also has **Trend** in navigation position 2. It opens **Barometer
Trend**, using only the latest `pages.baro-trend` snapshot, refreshed once per
minute while active:

```json
[{"bucket":"2026-11-07-8","min":1022.16,"max":1023.03,"average":1022.7911111111111}]
```

The feed supplies about three days of ordered, unique three-hour buckets in
hPa. Bucket numbers use local time: 1 is midnight-3 AM, through 8 at 9 PM-midnight.
All supplied buckets are plotted (including a 25-bucket snapshot), without
aggregation, sorting, gap filling, or trimming to exactly 24 bars. At most 72
buckets are supported; larger or malformed snapshots are logged and rejected
atomically, retaining any previous valid chart as stale.
Each floating bar spans `min` to `max`, with a contrasting white tick at the
supplied `average`. Ranges greater than 1.5 hPa are orange; ranges greater than
0.8 through 1.5 hPa are yellow; ranges at or below 0.8 hPa are green.
Equal min/max values remain visible. Missing/non-numeric values, inverted ranges,
averages outside the range, invalid dates/bucket numbers, and unordered or
duplicate buckets reject the snapshot. An empty array shows no pressure buckets.
The Y scale fits the supplied extrema with 1 hPa padding, and the footer counts
bars. Loading, stale, offline, memory-failure, and recovery behavior use the
shared snapshot-history page; no raw numeric-history subscription is made.
**History** in position 3 opens the 72-hour graph, which now has **Trend** in
position 3 to switch back. Back returns to Barometer; Home returns Home.

Wind uses the same monospaced layout with cyan **Time**, **Dir**, **Speed**,
and **Gust** headers. Rows show Current, 1 Hour, 2 Hours, 6 Hours, 12 Hours,
and 24 Hours. `pages.p03-wind` supplies six direction/speed/gust triples in
that order;
speed and gust are in mph. Missing values show `--`, and only changed rows
are redrawn. Back returns to Details; Home returns to Home.

Wind has a **Dir** button in navigation position 2. It opens
**Wind Direction**, a compass dial plus a text panel covering a rolling 12-hour
window. `src/screens/wind_direction_screen.cpp` reads only
`pages.winddir-graphic`, a precomputed JSON snapshot. It does not request raw
history or calculate wind statistics on the CYD. The external `winddir.py`
producer retrieves `currentwinddegrees` and `currentwindspeed` over one shared
rolling window, handles pagination and duplicate IDs, and pairs independently
calculated circular-mean bearings and arithmetic-mean speeds in UTC-aligned
10-minute buckets. Missing or undefined pairs are skipped, not filled.
Partial edge buckets mean a window can contain up to 73 pairs.

- **12h avg** (red rim marker) is the speed-weighted vector mean: the bearing
  of the sum of speed × unit vector. Strong winds count more than light ones.
  It is undefined when all readings are calm or the winds cancel exactly.
- **Steady** is the resultant vector length divided by the total speed:
  100% means one constant direction, and near 0% means no prevailing direction.
- **Range** (yellow arc outside the rim) is the smallest arc that covers every
  bearing. It is found by removing the largest empty gap between bearings.
- The blue petals form a 16-point wind rose. Each petal's length is
  proportional to the number of buckets from that sector, relative to the
  busiest sector.
- Buckets with a mean speed below 1 mph count as calm and are excluded from
  all of the above. The status line shows the calm share.

The white arrow points into the wind, toward the bearing the wind comes from,
like a weather vane. It turns gray when the current speed is calm. The current
bearing supplied by the producer is the newest valid direction history sample,
falling back to the latest `currentwinddegrees` value for an empty valid history.
The current speed comes from the latest `currentwindspeed` value. These current
readings are independent, not timestamp-paired. Current speed is rounded before
the display's calm test, preserving the existing behavior.
The feed refreshes every minute while the page is open, and the dial redraws
only when displayed values change. Invalid updates retain the previous graphic
and log the validation error. WiFi loss, a failed update, a receipt older than
two minutes, or a producer window older than two minutes show **Stale**.
Back returns to Wind, **Speed** opens the speed chart, and Home returns Home.

The `pages.winddir-graphic` feed value is a JSON object, for example:

```json
{"windowEndUtc":"2026-11-07T16:00:00Z","windowSeconds":43200,"bucketSeconds":600,"currentBearing":350,"currentSpeedMph":10,"hasAverage":true,"averageBearing":0,"steadiness":0.984807753,"rangeStart":350,"rangeSpan":20,"windySamples":2,"calmSamples":0,"sectorCounts":[2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}
```

All fields are required. Current bearing/speed may be `null` when unavailable;
`averageBearing` must be `null` when `hasAverage` is false. Bearings are in
degrees from north, speed is mph, and steadiness is a fraction from 0 to 1.
`rangeSpan` runs clockwise from `rangeStart`. Sector counts are nonnegative
integers ordered N, NNE, NE, ENE, E, ESE, SE, SSE, S, SSW, SW, WSW, W, WNW,
NW, NNW, summing to `windySamples`. The display derives their maximum.
An all-calm or empty window has no average, zero steadiness/range, and zero
sector counts; calm buckets contribute only to `calmSamples`.

The external Python producer runs on the Raspberry Pi with Python 3.10+,
`requests`, an accurate system clock, and environment variables
`AIO_USERNAME` and `AIO_KEY`; do not embed credentials in source. Create the
`pages.winddir-graphic` feed in Adafruit IO before publishing. From the
directory containing `winddir.py`, PowerShell examples are:

```powershell
$env:AIO_USERNAME = "your-username"
$env:AIO_KEY = "your-api-key"
python winddir.py --once --dry-run  # Read inputs and inspect JSON; no write.
python winddir.py --once           # Publish once.
python winddir.py                  # Publish every 60 seconds; Ctrl+C stops.
python -B -m unittest -v test_winddir
```

Every successful cycle publishes one snapshot. Retrieval or pagination failures
are logged and do not overwrite the feed with partial or fabricated results.
The producer retains full-precision statistical bearings; the CYD rounds labels.

Wind's navigation position 3 is **Speed**. It opens
**Last 72 Hours**, a two-line chart using only `pages.wind-line`, for example
`[{"ts":"2026110513","speed":4.0,"gust":7.0}, ...]`. Local calendar hours
identify hourly average speed (cyan) and maximum gust (yellow), both in mph.
Both series use the newest 72 elapsed hours, hourly buckets, and shared axes.
The producer fills missing hours before publishing; the display does not.
Leading `null` values remain unplotted without discarding the other series.
The Y axis
starts at 0 mph and extends 2 mph above the highest value. The summary line
shows a colored legend with each series' min-max range. Both series are
validated and published together from one latest-value request. Back returns to Wind,
**Dir** opens Wind Direction, and Home returns Home.

The Speed page's **Daily** button opens **Daily Wind - 28 Days**, using only
the latest `pages.om-wind` snapshot. It plots the supplied daily
`wind_speed_10m_mean` in cyan and `wind_gusts_10m_max` in yellow, in mph;
`wind_direction_10m_dominant` is ignored. Local `YYYYMMDD` dates and values
are plotted as supplied, without aggregation or gap filling. Null values leave
gaps independently, and zero is a reading. The date-based Past/Forecast
divider marks the start of today and moves at local midnight. The Y axis starts
at zero with 2 mph padding above the highest value. The feed is fetched on
entry and refreshed hourly; malformed or unordered snapshots retain the
previous chart as stale. Back returns to Speed and Home returns Home.

Temperatures uses the text-table layout for Temperature, Feels Like,
Apparent, Dew Point, Heat Index, and Wind Chill, in that order, from
`pages.p01-temp`. Feels Like and Apparent
labels are yellow, Heat Index and Wind Chill are pink, and the other labels
are cyan. Its columns are shifted left with values starting at x 130.
Values display one decimal place, such as `61.0 F (16.1 C)`; Snapshot retains
whole degrees. Missing values show `-- F`. Back returns to Details. Both pages poll the feed cache
and redraw only when their displayed data changes.

The former Temps 2 test page is now the main Temperatures page; the duplicate
Details button and route have been removed. It subscribes only to `pages.p01-temp`.
This feed is refreshed once per minute while active and contains one JSON array
of exactly six finite numbers, ordered Temperature, Feels Like, Apparent,
Dew Point, Heat Index, and Wind Chill, for example:
`[71.775, 70.75, 68.4, 63.349999999999994, 78.1, 75.5]`.
All six values come from the same snapshot. Missing data shows `-- F`;
malformed snapshots are logged and show `-- F` for every row rather than mixing
partial values.
Back returns to Details. Daily opens the existing daily chart, whose Back button
returns to Temperatures.

Temperatures' **History** button opens **Last 72 Hours**, using only the latest value
of `pages.temp-line` rather than downloading raw `feed-avg` history. This feed
contains a JSON array of hourly averages, for example
`[{"ts":"2026110409","ftemp":62.6},{"ts":"2026110410","ftemp":63.6}]`.
`ts` is a local date/hour (`YYYYMMDDHH`) in the display's `TZ_STRING` timezone;
`ftemp` is that hour's average Fahrenheit temperature. The graph shows 72 elapsed
hours ending just after the newest supplied hour, with one point per available
hour. Older entries are discarded. Points use the
same scale, min/max summary, and local endpoint labels as the previous chart.
The feed refreshes once per minute while active. Invalid snapshots are logged;
any last valid graph is retained and marked **Stale**. WiFi loss, a cache over two
minutes old, or a newest hour over two hours old also marks the graph stale.
Empty arrays show an empty-window message. Local timestamps cannot distinguish
the two fall-back hours, so an ambiguous hour uses standard time; nonexistent
spring-forward hours are rejected. Back returns to
Temperatures; Daily opens the existing daily chart; Home returns Home.

All five combined history feeds refresh once per minute while active. They
cancel any raw-history job on entry, decode the snapshot atomically,
skip copying/rebuilding unchanged feed revisions, and retain the last valid graph as **Stale** after
malformed updates, WiFi loss, or a cache older than two minutes. Hourly graphs
also become stale when the newest supplied hour is over two hours old;
daily graphs become stale when the snapshot no longer reaches local yesterday.
The producer guarantees oldest-to-newest order, unique timestamps, and filled
hourly readings. The display does not sort, deduplicate, aggregate, or fill
these feeds: it plots the received values within the requested window.
Explicit leading nulls remain unplotted. JSON syntax, numeric types, and chart
timestamps are still decoded/checked; malformed data is logged.

History decoding reads one JSON object at a time and retains at most 72 supplied
hourly readings per series or 30 daily readings, rather than building a JSON
document for the whole feed. Compact fixed-capacity chart storage avoids
throwing vector allocations. Checked staging/point allocations report
insufficient memory instead of aborting; a failed refresh retains the last valid
chart and retries after a subsequent feed poll. HTTPS buffers are released before
copying large responses, and a failed String copy is reported as a failure rather
than publishing an empty snapshot.

Astronomical shows Sunrise, Sunset, Daylight, Moonrise, Moonset, and Moon Phase using
`pages.p05-astro`, an array of five strings in sunrise, sunset, moonrise,
moonset, moon phase order. Times are already local clock strings such as
`6:23 AM`; moon phase is already a readable name such as `Waning Crescent`.
Missing values show
`--`. Sun labels are yellow and moon labels are cyan. Feeds are activated
when the page opens and the table redraws when displayed values change.
Back returns to Details; Home returns directly to Home.
Daylight is calculated from the sunrise and sunset clock values and displayed
as hours and minutes (`10:38` for `6:23 AM` to `5:01 PM`). Invalid sunrise/sunset
strings reject the snapshot with a logged error and show `--`.
Blank rows separate Daylight
from Moonrise and Moonset from Moon Phase.

**Weather outlook:** derives a simple weather descriptor from
  current and forecast pressure. Depending on pressure and pressure
  trend, Home displays Stormy, Clearing, Improving, Unsettled,
  Fair, or Nice. Active weather alerts override the descriptor
  with the existing alert button.
