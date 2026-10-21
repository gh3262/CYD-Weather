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
Adafruit IO manager, outside the widget; Temperature, Barometer, and Wind History use that path.

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
`refresh_policy` column is `periodic` or `on_demand`; periodic feeds specify
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
| Otherwise | Stable |

Both pressure feeds refresh every minute. Missing, nonnumeric, nonfinite,
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
| Wind History | Back | — | — | Home |
| Temperatures | Back | — | History | Home |
| Temperature History | Back | — | — | Home |
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
respectively. The top-right **Wind** button opens Wind. The remaining three slots
are inactive placeholders. The right third shows the
thermometer using `feed-avg` in red and `currenttemperatureapparent` in cyan,
on a 10 to 100 F scale. The tube is centered in its panel, with Fahrenheit
ticks on the left and Celsius ticks on the right (enabled by
`ThermometerWidgetOptions::showCelsiusScale`). Until a temperature is available it shows `-- F`
instead of a default reading.

Barometer uses a monospaced three-column table with cyan **Time**, **Pressure**,
and **Trend** headers. Its rows show Current, 1 Hour, 2 Hours, 6 Hours, 12 Hours,
and 24 Hours using the corresponding pressure/trend feeds. Pressure is in hPa;
unavailable values show `--`. Only changed rows are redrawn as feeds arrive.
Back returns to Details.

Precipitation has a **History** button in navigation position 3. It opens
**Precip - 14 Days**, a cyan daily bar chart of `omp-totprec` in inches.
The window covers the previous 14 completed calendar dates according to the
device's local clock, excluding local today. Records still keep their original
UTC date labels; only the choice of the last displayed date uses local time.
Each record's `created_at` date identifies the day: duplicate daily
totals are **not summed or averaged**; the largest valid amount for each date
is plotted. Dates without records are shown as zero. Dry dates (including missing
dates) have a yellow 3-pixel bar so all 14 days can be counted; precipitation
bars remain cyan. Bars occupy 75% of each daily slot, leaving a small gap.
These visual zero markers do not contribute to the total or peak.
The Y axis starts at zero,
uses an automatic fractional-inch scale, and keeps small positive amounts
visible. The summary shows the sum of the 14 daily maxima and the largest daily
amount, to three decimal places. Bottom labels show the first and last recorded
dates without local-time conversion.

Retrieval uses the full date range in 100-record pages, not a fixed record cap.
The supplied export's last 14 dates contain 141 records (3-11 per date), so
140 records would not reliably cover the window. Pagination completes before
displaying any result. A successfully empty window draws an all-zero chart;
loading and failed requests remain distinct. Refresh failures or WiFi loss mark
retained data **Stale**; crossing local midnight also marks yesterday's window
stale until the new window loads. Back returns to Precipitation; Home returns
Home. Leaving cancels the history subscription.

Barometer has a **History** button in navigation position 3. It opens
**Last 72 Hours**, a `currentpressure` chart that uses the same shared history
page as Temperature History (`screens/history_chart_page`), with the same
loading, empty, stale, and cancellation behavior. Readings are averaged into
2-hour UTC buckets over a rolling 72-hour window. The Y range is the observed
minimum minus 2 hPa through the observed maximum plus 2 hPa; label spacing
(1, 2, 5, 10, 20, or 50 hPa) is the finest that gives at most eight labels.
Back returns to Barometer; Home returns Home.

Wind uses the same monospaced layout with cyan **Time**, **Dir**, **Speed**,
and **Gust** headers. Rows show Current, 1 Hour, 2 Hours, 6 Hours, 12 Hours,
and 24 Hours. Direction comes from the corresponding wind-direction feed;
speed and gust are in mph. Missing values show `--`, and only changed rows
are redrawn. Back returns to Details; Home returns to Home.

Wind has a **Dir** button in navigation position 2. It opens
**Wind Direction**, a compass dial plus a text panel covering a rolling 12-hour
window. `src/screens/wind_direction_screen.cpp` requests two numeric-history
series with 10-minute UTC buckets: `currentwinddegrees` (CircularAverage) and
`currentwindspeed` (Average). It pairs each bucket's mean bearing with that
bucket's mean speed and passes the pairs to the pure functions in
`src/screens/wind_direction_stats.*`:

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
bearing is the newest history sample, or the latest `currentwinddegrees` value
before the history loads. The speed comes from `currentwindspeed`. History
refreshes every minute while the page is open, and the dial redraws only when
displayed values change. Failures, WiFi loss, or data more than two minutes old
show **Stale**. Back returns to Wind, **Speed** opens the speed chart, and Home
returns Home.

Wind's navigation position 3 is **Speed**. It opens
**Last 48 Hours**, a two-line chart on the shared history page over a rolling
48-hour window with 2-hour UTC buckets. The cyan line is the bucket average of
`currentwindspeed`; the yellow line is the bucket maximum (peak gust) of
`currentwindgust`. Both series use the same window and shared axes. The Y axis
starts at 0 mph and extends 2 mph above the highest value. The summary line
shows a colored legend with each series' min-max range. Both feeds are fetched
one after the other, so loading takes about twice as long as a single-feed
chart. The chart appears only after both series load. Back returns to Wind;
Home returns Home.

Temperatures uses the text-table layout for `feed-avg`, `wapi.feels-like`,
`currenttemperatureapparent`, `currenttemperaturedewpoint`,
`wapi.heat-index`, and `wapi.wind-chill`, in that order. Feels Like and Apparent
labels are yellow, Heat Index and Wind Chill are pink, and the other labels
are cyan. Its columns are shifted left with values starting at x 130.
Values display one decimal place, such as `61.0 F (16.1 C)`; Snapshot retains
whole degrees. Missing values show `-- F`. Back returns to Details. Both pages poll the feed cache
and redraw only when their displayed data changes.

Temperatures has a **History** button in navigation position 3. It opens
**Last 24 Hours**, a page-wide `feed-avg` chart covering a timestamp-verified
rolling 24-hour window. Available samples are averaged by UTC hour, including
partial first/current hours, and plotted at the midpoint of each bucket's
visible interval. Empty hours are gaps; data is not carried forward or
interpolated. The Y range is the raw valid minimum minus 10 F through the raw
valid maximum plus 10 F. The actual observed minimum and maximum temperatures
are displayed above the chart, with Y labels every 10 F and ticks every 5 F.
The bottom labels show the window's local date/time endpoints and valid sample
count. Local labels use `TZ_STRING`; UTC spacing remains correct across DST.
Loading, failed, and successfully empty windows have distinct messages.
Failures, WiFi loss, or a window over two minutes old mark retained chart data
**Stale** rather than erase it. Back returns to Temperatures; Home returns Home.
Leaving the page cancels chart retrieval without changing latest-value polling.

Astronomical shows Sunrise, Sunset, Daylight, Moonrise, Moonset, and Moon Phase using
the five `days1sun*`/`days1moon*` feeds. UTC timestamps are displayed in local
US Central time (including daylight saving time) as `03:21 PM`; moon phase
values use readable names such as `Waxing Crescent`. Missing values show
`--`. Sun labels are yellow and moon labels are cyan. Feeds are activated
when the page opens and the table redraws when displayed values change.
Back returns to Details; Home returns directly to Home.
Daylight is the difference between the sunrise and sunset Central-time clock
values, ignoring their dates because the feeds describe the next events.
It is an estimate when the events fall on different days. The duration is
displayed as hours and minutes (for example, `11:03`), dropping partial minutes;
missing or unparseable timestamps show `--`. Blank rows separate Daylight
from Moonrise and Moonset from Moon Phase.

**Weather outlook:** derives a simple weather descriptor from
  current and forecast pressure. Depending on pressure and pressure
  trend, Home displays Stormy, Clearing, Improving, Unsettled,
  Fair, or Good. Active weather alerts override the descriptor
  with the existing alert button.
