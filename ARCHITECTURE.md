# Architecture

Scope: all 74 `.h`/`.cpp` files currently present under `include/` and `src/`.
Matching header/implementation files share a row; header-only and implementation-only
files are explicitly identified. Layers describe responsibilities, not enforced
dependency boundaries. Configuration values and credentials are intentionally omitted.

## 1. Hardware/Display

| File Pair | Primary Responsibility (1 sentence) | State/Memory Owned (structs, sprites, timers) | Key Callers/Callees |
|---|---|---|---|
| [include/cyd.h](include/cyd.h) (header-only) | Initializes CYD peripherals and supplies splash-screen and network-status LED helpers. | No persistent state. | Called by `main.cpp`; calls `cyd_helpers.h`, `cyd_display.h`, `cyd_touch.h`. |
| [include/cyd_pins.h](include/cyd_pins.h) (header-only) | Defines hardware GPIO assignments and RGB565 color constants. | Compile-time GPIO/color macros; no runtime buffers. | Used by CYD headers, `cyd_globals.cpp`, `sound_manager.cpp`. |
| [include/cyd_theme.h](include/cyd_theme.h) (header-only) | Defines shared colors, font selections, screen geometry, and touch/navigation dimensions. | Compile-time theme/layout constants; no runtime state. | Used by display, touch, screens, widgets through `app.h`/`cyd.h`. |
| [include/cyd_helpers.h](include/cyd_helpers.h) (header-only) | Provides GPIO/backlight control and uptime, temperature, and moon-phase formatting helpers. | Static uptime text buffer; static moon-phase name table; temporary `String` values. | Called by `cyd.h`, `main.cpp`, detail/diagnostic screens; calls Arduino GPIO/ADC functions. |
| [include/cyd_display.h](include/cyd_display.h) (header-only) | Renders shared title bars, clock, alert indicator, buttons, and text using the display object. | `extern Arduino_GFX *gfx`; `extern bool headerAlertActive` (definitions elsewhere); shared display cursor/font/color state via `gfx`. | Called by screens/widgets and `main.cpp`; calls `gfx`, `cyd_time.h`, `cyd_theme.h`. |
| [src/cyd_globals.cpp](src/cyd_globals.cpp) (implementation-only) | Constructs the single shared TFT bus, display, and touchscreen instances. | Heap-allocated `displayBus: Arduino_ESP32SPI`; heap-allocated `gfx: Arduino_ILI9341`; global `touch: XPT2046_Touchscreen`; no application sprite/framebuffer. | Implements externs in `cyd_display.h`/`cyd_touch.h`; calls external Arduino GFX and touchscreen constructors. |
| [include/cyd_touch.h](include/cyd_touch.h) (header-only) | Converts raw touch readings to calibrated screen coordinates and performs audible button hit tests. | `TouchCalibration`; function-static calibration and active flag; temporary `Preferences` handle; calibration persisted in NVS; external `touch` instance. | Called by `main.cpp` and screens; calls XPT2046, SPI, Preferences, `sound_manager`. |
| [src/audio/sound_manager.h](src/audio/sound_manager.h) + [src/audio/sound_manager.cpp](src/audio/sound_manager.cpp) | Plays LEDC-driven tones and sequences and controls sound volume and button beeps. | `Sound`, `ToneStep`; fixed `steps[SOUND_MAX_STEPS]`; step index/count/start time; initialized/enabled/playing/beep flags; volume; LEDC channel 6/timer 3. | Called by `main.cpp`, `cyd_touch.h`, System; calls ESP32 LEDC and speaker GPIO. |
| [src/widgets/line_chart_widget.h](src/widgets/line_chart_widget.h) + [src/widgets/line_chart_widget.cpp](src/widgets/line_chart_widget.cpp) | Draws clipped line, bar, floating-range, precipitation, and pressure-trend charts with shared axes and overlays. | `LineChartPoint`, `LineChartWidgetOptions`, `LineChartSeries`; local `ScaleSteps` and geometry; caller-owned point buffers; no retained chart data or sprite. | Called by `history_chart_page.cpp`; calls `gfx` primitives through `app.h` and external Arduino GFX. |
| [src/widgets/thermometer_widget.h](src/widgets/thermometer_widget.h) + [src/widgets/thermometer_widget.cpp](src/widgets/thermometer_widget.cpp) | Draws the temperature/apparent-temperature thermometer and Fahrenheit/Celsius scales. | `ThermometerWidgetOptions`; local geometry; no retained readings, timer, or sprite. | Called by `details_screen.cpp`; calls `gfx` through `app.h`. |

## 2. Network/BLE

No BLE `.h`/`.cpp` implementation is present under `src/bluetooth/`.
WiFi connection selection/retry is implemented in `src/main.cpp`, listed under UI Manager.

| File Pair | Primary Responsibility (1 sentence) | State/Memory Owned (structs, sprites, timers) | Key Callers/Callees |
|---|---|---|---|
| [include/credentials.h](include/credentials.h) + [include/credentials_template.h](include/credentials_template.h) (configuration headers, not an implementation pair) | Supply deployment configuration and its editable template for WiFi, Adafruit IO, NTP, and timezone settings. | `WiFiCredential`; configured network array/count and configuration macros; values omitted here. | Consumed by `main.cpp`, `adafruitio_manager.cpp`, `cyd_time.h`; template is copied, not compiled alongside the configured header. |
| [include/cyd_time.h](include/cyd_time.h) (header-only) | Synchronizes SNTP time and formats local clock/date and UTC timestamp values. | Function-static start-date/time buffer, volatile NTP-reply flag, formatting buffers; ESP32 SNTP clock/timezone configuration. | Called by `main.cpp`, `cyd_display.h`, Tools/System/WiFi screens; calls `esp_sntp`, `configTime`, C time functions, configuration header. |
| [src/adafruitio/adafruitio_manager.h](src/adafruitio/adafruitio_manager.h) + [src/adafruitio/adafruitio_manager.cpp](src/adafruitio/adafruitio_manager.cpp) | Polls authenticated HTTPS feeds and numeric history while publishing synchronized cache snapshots to the UI. | `AdafruitIOManager`, private `FeedCache[117]`; `AdafruitIOFeedDefinition`, `AdafruitIOValueType`, `AdafruitIORefreshPolicy`, `NumericHistoryState`, `NumericHistoryStatus`; feed strings/history vectors/revisions/receipt times; mutex; core-0 worker task (8192-byte stack); 60-entry request-time ring and throttle/retry timers; two-series numeric requests/results/staging, accumulator, generation, cursor/page count; per-request TLS/HTTP/JSON objects. | Started/queried by `main.cpp`; configured/read by screens and shared pages; uses `feed_catalog.cpp`, `numeric_history.*`, `numeric_history_page.h`, HTTPClient, WiFiClientSecure, ArduinoJson, FreeRTOS. |
| [src/adafruitio/feed_catalog.cpp](src/adafruitio/feed_catalog.cpp) (implementation-only; declarations in manager header) | Registers feed keys, types, units, long-value capability, and refresh policies/intervals. | Constant `AIO_FEEDS[]`, `AIO_FEED_COUNT`; compile-time capacity assertion. | Read by `adafruitio_manager.cpp` and `snapshot_history_page.cpp`; definitions use `adafruitio_manager.h`. |

## 3. Data Models

| File Pair | Primary Responsibility (1 sentence) | State/Memory Owned (structs, sprites, timers) | Key Callers/Callees |
|---|---|---|---|
| [src/adafruitio/numeric_history.h](src/adafruitio/numeric_history.h) + [src/adafruitio/numeric_history.cpp](src/adafruitio/numeric_history.cpp) | Parses UTC timestamps/cursors and accumulates raw API readings into requested numeric-history windows. | `HistoryAggregation`, `HistoryWindow`, `NumericHistoryRequest`, `NumericHistoryBucket`, `NumericHistoryResult`; result bucket vector; `NumericHistoryAccumulator::_result`, `_oldestMs`, `_boundaryIds` vector. | Called by manager/history-page decoder and snapshot date/time helpers; no display calls. |
| [src/adafruitio/numeric_history_page.h](src/adafruitio/numeric_history_page.h) (header-only) | Decodes and validates a filtered API history page before feeding its records to the accumulator. | `NUMERIC_HISTORY_PAGE_SIZE = 100`; temporary JSON filter/document and timestamp/cursor variables; caller-owned accumulator/cursor. | Called by `adafruitio_manager.cpp::fetchNumericHistoryPage`; calls ArduinoJson and `numeric_history.*`. |
| [src/screens/weather_page_values.h](src/screens/weather_page_values.h) (header-only) | Validates combined Wind, Barometer, and Astronomical arrays and computes daylight duration. | `WeatherPageValues<N>` = `array<array<char,64>,N>`; aliases `WindPageValues` (18 entries), `BarometerPageValues` (12), `AstronomicalPageValues` (5); temporary parsed arrays/JSON document; no retained state. | Called by Wind, Barometer, Astronomical; calls ArduinoJson and clock/numeric parsers. |
| [src/screens/temperature_page_values.h](src/screens/temperature_page_values.h) (header-only) | Validates the six-value combined temperature snapshot before replacing output values. | `TemperaturePageValues = array<float,6>`; temporary parsed array/JSON document; caller-owned output. | Called by Temperatures; calls ArduinoJson. |
| [src/screens/precipitation_page_values.h](src/screens/precipitation_page_values.h) (header-only) | Validates and formats the 21 chance/type/amount values in the combined precipitation snapshot. | `PrecipitationPageValues = array<array<char,64>,21>`; temporary parsed array/JSON document; no retained state. | Called by Precipitation; calls ArduinoJson and numeric/string helpers. |
| [src/screens/aqi_page_values.h](src/screens/aqi_page_values.h) (header-only) | Validates AQI strings/colors/timestamps and formats the supplied timestamp without shifting its wall-clock time. | Caller-owned `JsonDocument`; temporary calendar/timestamp buffers and numeric variables; no retained state. | Called by AQI; calls `numeric_history.cpp::parseHistoryTimestamp` and ArduinoJson. |
| [src/screens/weather_snapshot_history.h](src/screens/weather_snapshot_history.h) (header-only) | Streams combined history records, validates date/value fields, and stages bounded chart results. | `SnapshotHistoryField`, `SnapshotHistoryBucket`, `SnapshotHistoryBuckets` with 72-entry `std::array`, `SnapshotHistoryResult`, `SnapshotJsonReader`; checked heap staging arrays; one temporary JSON object per record; caller-owned output. | Called by snapshot-history adapters and specialized parsers; uses `numeric_history.*`, ArduinoJson, C time functions. |
| [src/screens/daily_forecast_history.h](src/screens/daily_forecast_history.h) (header-only) | Decodes ordered daily forecast series with range and optional percentage validation. | Temporary checked heap `SnapshotHistoryResult[seriesCount]`; count/date/error locals; caller-owned results. | Called by `snapshot_history_page.cpp` and `clouds_history.h`; calls `visitSnapshotReadings`. |
| [src/screens/temperature_hourly_history.h](src/screens/temperature_hourly_history.h) (header-only) | Provides convenience wrappers for parsing hourly temperature history and local hour keys. | Local `SnapshotHistoryField[]`; caller-owned `SnapshotHistoryResult`; no persistent state. | Used by native tests; no current production caller; delegates to `weather_snapshot_history.h`. |
| [src/screens/temperature_hourly_forecast.h](src/screens/temperature_hourly_forecast.h) (header-only) | Decodes a single local day's temperature/bounds series and calculates the current forecast-hour cutoff. | Temporary checked heap `SnapshotHistoryResult[3]`; at most 24 populated hourly slots per series; local date/count/error variables; caller-owned results. | Called by `snapshot_history_page.cpp`, `history_chart_page.cpp`; calls `weather_snapshot_history.h`, `numeric_history.*`, C time functions. |
| [src/screens/precipitation_daily_history.h](src/screens/precipitation_daily_history.h) (header-only) | Converts daily wet hours and precipitation composition into cumulative chart series. | Temporary checked heap `SnapshotHistoryResult[3]`; four input fields and three cumulative values per record; caller-owned results. | Called by `snapshot_history_page.cpp`; calls `visitSnapshotReadings`. |
| [src/screens/barometer_trend_history.h](src/screens/barometer_trend_history.h) (header-only) | Validates three-hour pressure buckets and stages their minimum, maximum, and average series. | `PRESSURE_TREND_SERIES_COUNT = 3`; temporary checked heap results, streamed JSON object, bucket timestamps; caller-owned results. | Called by `snapshot_history_page.cpp`; calls `SnapshotJsonReader`, local-hour parser, ArduinoJson, C time functions. |
| [src/screens/clouds_history.h](src/screens/clouds_history.h) (header-only) | Selects sunshine and mean-cloud percentages from a daily forecast snapshot. | Local two-entry `SnapshotHistoryField[]`; caller-owned results; no persistent state. | Called by `snapshot_history_page.cpp`; delegates to `daily_forecast_history.h`. |
| [src/screens/wind_direction_stats.h](src/screens/wind_direction_stats.h) + [src/screens/wind_direction_stats.cpp](src/screens/wind_direction_stats.cpp) | Calculates wind rose sectors, weighted bearing, steadiness, and covering range from samples. | `WindDirectionSample`, `WindDirectionStats` with 16 sector counts; local bearing vector and accumulators; static compass-name table; no retained sample history. | `wind_direction_screen.cpp` uses `windCompassPoint`; `wind_direction_snapshot.h` uses the types; native tests call the sample-statistics calculator. |
| [src/screens/wind_direction_snapshot.h](src/screens/wind_direction_snapshot.h) (header-only) | Validates the compact producer-calculated wind-direction snapshot before replacing output. | `WindDirectionSnapshot`; embedded `WindDirectionStats`; temporary parsed snapshot/JSON document; caller-owned output. | Called by Wind Direction; calls `numeric_history.*`, `wind_direction_stats.h`, ArduinoJson. |
| [src/screens/weather_descriptor.h](src/screens/weather_descriptor.h) (header-only) | Derives a short weather-outlook label from current and forecast pressure. | No mutable state; constexpr predicates and string literals. | Called by Home; no network or drawing calls. |

## 4. UI Manager

| File Pair | Primary Responsibility (1 sentence) | State/Memory Owned (structs, sprites, timers) | Key Callers/Callees |
|---|---|---|---|
| [include/app.h](include/app.h) + [src/main.cpp](src/main.cpp) (logical application interface/entry point) | Boots the firmware, maintains WiFi, dispatches screens/touches, and schedules UI updates. | `Screen`, `AppState`, global `state`, global `adafruitIOManager`, global `headerAlertActive`; `ScreenDef screens[]`; `StrongestKnownNetwork` scan result; 250 ms screen-poll timer/revision/pending flag; WiFi retry/link flags and timestamps; touch rearm/last-touch state; five-minute backlight-idle state; last clock minute. | Arduino calls `setup`/`loop`; calls CYD init, sound, time, manager, every registered screen, and Tools' `updateBackgroundJobs`; screens call `drawCurrentScreen`. |
| [src/screens/text_table_page.h](src/screens/text_table_page.h) + [src/screens/text_table_page.cpp](src/screens/text_table_page.cpp) | Draws shared label/value tables and redraws only changed rows while dispatching configured buttons. | `TextTableColor`, `TextTableRow`, `TextTableButton`, `TextTablePage`; `RenderedTableRow renderedRows[8]`; cache-valid/count/column/background fields; cached label/value `String`s. | Called by Snapshot, Temperatures, Astronomical; button type also used by Details and text blocks; calls `cyd_display.h`, `cyd_touch.h`, `gfx`. |
| [src/screens/text_block_page.h](src/screens/text_block_page.h) + [src/screens/text_block_page.cpp](src/screens/text_block_page.cpp) | Wraps and pages long feed text using a reusable eight-visible-line renderer and configurable navigation. | `TextBlockPage`, private `WrappedLine`; `cachedSource`, sanitized `cachedText`, `vector<WrappedLine> cachedLines`, `layoutCached`; caller-owned first-line/page selection. | Called by Summary, Forecast, Alerts; uses `TextTableButton`/colors, CYD display/touch helpers, `gfx`. |
| [src/screens/snapshot_history_page.h](src/screens/snapshot_history_page.h) + [src/screens/snapshot_history_page.cpp](src/screens/snapshot_history_page.cpp) | Adapts active combined-feed snapshots into parsed chart data with revision and staleness tracking. | Shared `snapshotHistories[3]`, `snapshotStatus`, feed/field pointers, revision/attempt time, parsed/active flags, stale-age threshold, reference-line value; temporary payload `String`. | Called by all current history/forecast chart screens; calls manager getters/subscription methods, specialized parsers, `history_chart_page.*`; cancels raw numeric subscriptions on entry. |
| [src/screens/history_chart_page.h](src/screens/history_chart_page.h) + [src/screens/history_chart_page.cpp](src/screens/history_chart_page.cpp) | Builds chart frames, maps history results to widget points, and renders chart summaries, dates, and stale indicators. | `HistoryChartScale`, `HistoryChartSeries`, `HistoryChartStyle`, `HistoryChartPage`; active/rendered revision/stale/data/day/hour cache; temporary checked `unique_ptr<LineChartPoint[]>` buffers for up to three series; optional raw numeric-history result copies. | Called by `snapshot_history_page.cpp`; calls `line_chart_widget.*`, manager, time helpers, `gfx`; `main.cpp` calls `leaveHistoryChartPage`; raw-history entry API remains available but current screens use snapshots. |
| [src/screens/barometer_history_scale.h](src/screens/barometer_history_scale.h) (header-only) | Selects pressure-chart tick/label spacing with a bounded number of Y-axis labels. | Static constant `HistoryChartScale SCALES[]`; no mutable state. | Called by Barometer History and Barometer Trend; uses `history_chart_page.h`. |

## 5. Individual Screens

Screen lifecycle declarations are in `include/app.h`; these `.cpp` files do not
have matching per-screen headers. `src/main.cpp::screens[]` calls their draw,
touch, and refresh entry points, and centrally handles Home navigation where enabled.

| File Pair | Primary Responsibility (1 sentence) | State/Memory Owned (structs, sprites, timers) | Key Callers/Callees |
|---|---|---|---|
| [src/screens/home_screen.cpp](src/screens/home_screen.cpp) (implementation-only) | Displays the clock, temperature, pressure-derived outlook or alert shortcut, and primary navigation. | Cached temperature/weather `String`s, rendered alert count, invalid-pressure log flag; local feed values; no sprite. | `main.cpp`; calls manager, `weather_descriptor.h`, CYD display/time helpers; opens Snapshot, Forecast, Details, System, Alerts. |
| [src/screens/details_screen.cpp](src/screens/details_screen.cpp) (implementation-only) | Provides the weather-detail button grid and live thermometer panel. | Constant `TextTableButton DETAIL_BUTTONS[8]`; cached actual/apparent `String`s and force-render flag; local `ThermometerWidgetOptions`. | Home/child Back routes and `main.cpp`; calls manager for `feed-avg`/apparent temperature, `thermometer_widget.*`, detail-screen entry functions. |
| [src/screens/snapshot_screen.cpp](src/screens/snapshot_screen.cpp) (implementation-only) | Combines individual current-weather feeds into an eight-row snapshot table. | Feed/label/color arrays; current/rendered `String[8]`; force-render flag; local `TextTableRow[]`. | Home and `main.cpp`; calls manager, temperature helpers, `text_table_page.*`; opens Summary. |
| [src/screens/summary_screen.cpp](src/screens/summary_screen.cpp) (implementation-only) | Shows and pages the `om-current` weather narrative. | `summaryText`; first-line/line-count indices; scrollable flag; local `TextBlockPage`. | Snapshot and `main.cpp`; calls manager, `text_block_page.*`; opens Forecast. |
| [src/screens/forecast_screen.cpp](src/screens/forecast_screen.cpp) (implementation-only) | Browses the 14 NWS forecasts with initial loading, directional prefetch, paging, and day/night styling. | Forecast index/text/title buffer; first-line/count/scrollable state; `forecastLoaded[FORECAST_COUNT]`; feed-key array. | Home/Summary and `main.cpp`; calls manager for `nws.00` through `nws.13`, `text_block_page.*`. |
| [src/screens/alerts_screen.cpp](src/screens/alerts_screen.cpp) (implementation-only) | Implements the alert summary, individual alert details, and full-alert scrolling views. | Alert title/text/count; detail index/count; first-line/line-count; request-failed flag; local `TextBlockPage`. | Home and `main.cpp`'s three alert screen entries; calls manager latest/history APIs, `text_block_page.*`. |
| [src/screens/temperatures_screen.cpp](src/screens/temperatures_screen.cpp) (implementation-only) | Displays the six combined temperature readings and temperature-chart shortcuts. | Current/rendered `String[6]`, last payload, force flag; local `TemperaturePageValues` and `TextTableRow[]`; label/color constants. | Details and temperature-chart Back routes; calls manager for `pages.p01-temp`, `temperature_page_values.h`, `text_table_page.*`; opens Daily/History. |
| [src/screens/temperature_history_screen.cpp](src/screens/temperature_history_screen.cpp) (implementation-only) | Configures the 72-hour temperature line chart and its Daily/Hourly navigation. | Constant series/field arrays; local `HistoryChartPage`; retained data owned by shared history modules. | Temperatures/Daily/Hourly and `main.cpp`; calls `snapshot_history_page.*` for `pages.temp-line`, with a 32 F reference line. |
| [src/screens/temperature_daily_screen.cpp](src/screens/temperature_daily_screen.cpp) (implementation-only) | Configures the 28-day low/high temperature forecast-range chart. | Constant series/field arrays; local page/scale values; no private history buffer. | Temperatures/History/Hourly and `main.cpp`; calls `snapshot_history_page.*` for `pages.om-temp`; opens Hourly/History. |
| [src/screens/temperature_hourly_screen.cpp](src/screens/temperature_hourly_screen.cpp) (implementation-only) | Configures the 24-hour temperature line and lower/upper forecast band chart. | Constant three-series array; local page/scale values; no private history buffer. | Daily/History and `main.cpp`; calls `snapshot_history_page.*` for `pages.om-temp-avg`; opens Daily/History. |
| [src/screens/precipitation_screen.cpp](src/screens/precipitation_screen.cpp) (implementation-only) | Displays the seven-row combined precipitation chance/type/amount table. | `PrecipitationRow`; current/rendered `String[7][3]`; last payload and force flag; local `PrecipitationPageValues`. | Details/chart Back routes and `main.cpp`; calls manager for `pages.p02-precipitation`, parser, direct `gfx` table drawing; opens Daily/History. |
| [src/screens/precipitation_daily_screen.cpp](src/screens/precipitation_daily_screen.cpp) (implementation-only) | Configures the 28-day precipitation-hours chart with rain/showers/snow composition. | Constant three-series array; local page/scale values; shared history storage only. | Precipitation/History and `main.cpp`; calls `snapshot_history_page.*` for `pages.om-precip`; opens History. |
| [src/screens/precipitation_history_screen.cpp](src/screens/precipitation_history_screen.cpp) (implementation-only) | Configures the 30-day daily-total precipitation bar chart and adaptive fractional-inch scale. | Constant series/field arrays; local page and calculated scale; shared history storage only. | Precipitation/Daily and `main.cpp`; calls `snapshot_history_page.*` for `pages.prcp-line`; opens Daily. |
| [src/screens/wind_screen.cpp](src/screens/wind_screen.cpp) (implementation-only) | Displays current and forecast direction, speed, and gust values in a fixed-column table. | `WindRow`; rendered value `String` matrix; last payload/force flag; local `WindPageValues`; column constants. | Details/wind-child Back routes and `main.cpp`; calls manager for `pages.p03-wind`, `weather_page_values.h`, direct `gfx`; opens Direction/History. |
| [src/screens/wind_history_screen.cpp](src/screens/wind_history_screen.cpp) (implementation-only; also implements Daily Wind) | Configures both the 72-hour speed/gust chart and the 28-day daily wind forecast chart. | Separate constant history/daily series and field arrays; constant scale choices; local `HistoryChartPage`s; no private history buffer. | Wind/Direction/Daily Back routes and `main.cpp`'s History/Daily entries; calls `snapshot_history_page.*` for `pages.wind-line` and `pages.om-wind`. |
| [src/screens/wind_direction_screen.cpp](src/screens/wind_direction_screen.cpp) (implementation-only) | Renders the compact wind-direction snapshot as a wind rose, bearing/range markers, and statistics panel. | `WindDirectionView`; retained `WindDirectionSnapshot snapshot`, `renderedView`; active/parsed/data/failed/rendered flags; feed revision/attempt timestamp; local geometry. | Wind/History and `main.cpp`; calls manager for `pages.winddir-graphic`, snapshot parser, `windCompassPoint`, direct `gfx`; cancels raw numeric history and exposes leave cleanup. |
| [src/screens/barometer_screen.cpp](src/screens/barometer_screen.cpp) (implementation-only) | Displays the combined pressure and trend table for current and forecast times. | `BarometerRow`; rendered `String[][2]`; last payload/force flag; local `BarometerPageValues`; column constants. | Details/chart Back routes and `main.cpp`; calls manager for `pages.p04-baro`, `weather_page_values.h`, direct `gfx`; opens History. |
| [src/screens/barometer_history_screen.cpp](src/screens/barometer_history_screen.cpp) (implementation-only) | Configures the 72-hour pressure line chart with a Trend shortcut. | Constant series/field arrays; local page; shared history storage only. | Barometer/Trend and `main.cpp`; calls `snapshot_history_page.*` for `pages.baro-line`, `barometer_history_scale.h`; opens Trend. |
| [src/screens/barometer_trend_screen.cpp](src/screens/barometer_trend_screen.cpp) (implementation-only) | Configures three-hour pressure-range bars with average ticks and reciprocal History navigation. | Constant three-series/field arrays; local page; shared history storage only. | Barometer History and `main.cpp`; calls `snapshot_history_page.*` for `pages.baro-trend`, `barometer_history_scale.h`. |
| [src/screens/clouds_screen.cpp](src/screens/clouds_screen.cpp) (implementation-only) | Configures the daily sunshine bar and mean-cloud line overlay chart. | Constant two-series array; local page/fixed scale; shared history storage only. | Details and `main.cpp`; calls `snapshot_history_page.*` for `pages.om-sun`; Back opens Details. |
| [src/screens/astronomical_screen.cpp](src/screens/astronomical_screen.cpp) (implementation-only) | Displays sunrise, sunset, daylight, moonrise, moonset, and moon phase. | Current/rendered `String[5]`, daylight/current-rendered strings, last payload/force flag; local `AstronomicalPageValues`, row array, daylight minutes. | Details and `main.cpp`; calls manager for `pages.p05-astro`, `weather_page_values.h`, `text_table_page.*`. |
| [src/screens/aqi_screen.cpp](src/screens/aqi_screen.cpp) (implementation-only) | Draws colored Now/Today/Tomorrow AQI cards, categories, optional pollutant lines, and the feed timestamp. | Last payload `String`, force/availability flags; temporary `JsonDocument`, RGB values, wrapped strings, timestamp buffer; no sprite or private network timer. | Details and `main.cpp`; calls manager for `pages.p06-aqi`, `aqi_page_values.h`, CYD font/navigation helpers, direct `gfx`; 30-minute feed interval belongs to catalog/manager. |
| [src/screens/system_screen.cpp](src/screens/system_screen.cpp) (implementation-only) | Displays memory/build/uptime diagnostics and provides WiFi, Tools, refresh, and beep controls. | Button geometry constants; temporary ESP heap/sketch values and worker-stack byte count; beep setting owned by sound manager. | Home and `main.cpp`; calls ESP/heap APIs, manager stack-headroom getter, CYD/time helpers, sound manager; opens WiFi/Tools. |
| [src/screens/wifi_status_screen.cpp](src/screens/wifi_status_screen.cpp) (implementation-only) | Displays WiFi identity/signal, uptime, and shared ping results with a refresh action. | Temporary WiFi/status values; ping results owned by global `AppState`; no private task/timer. | System and `main.cpp`; calls WiFi, time/display helpers, Tools' `startPings`; Back opens System. |
| [src/screens/tools_screen.cpp](src/screens/tools_screen.cpp) (implementation-only) | Runs ping and NTP diagnostics without blocking the main UI and exposes touchscreen calibration. | Ping active/volatile-finished flags; transient FreeRTOS ping task (6144-byte stack); NTP active/start-time/status; shared `AppState` ping fields. | System/WiFi and `main.cpp::loop`; calls ESP32Ping, WiFi, `cyd_time.h`, `refreshCurrentScreenResults`; opens calibration. |
| [src/screens/touch_calibration_screen.cpp](src/screens/touch_calibration_screen.cpp) (implementation-only) | Captures four raw touch targets and saves the resulting calibration. | `TS_Point calibrationPoints[4]`; step index, finished/saved flags; target-coordinate arrays; temporary `TouchCalibration`. | Tools and `main.cpp`; calls raw `touch`, `saveTouchCalibration` in `cyd_touch.h`, direct `gfx`; returns to Tools. |

## Feed update to pixels

The transport is HTTPS polling, not MQTT/BLE push. The worker publishes cache
changes; the Arduino loop performs rendering. The following concrete path uses
the Temperatures detail page while it is visible.

```text
Adafruit IO HTTPS response: pages.p01-temp, /data/last
  |
  v  FreeRTOS worker task
src/adafruitio/adafruitio_manager.cpp
  workerTask() -> runWorker() -> fetchLatestValues()
  HTTPClient::GET() -> deserializeJson(http.getStream(), value filter)
  fetchLatestValues() returns vector<String>
  runWorker() -> publishLatestValues()
  lock mutex -> replace FeedCache value -> increment revision if changed
  unlock mutex
  |
  | shared cache/revision (not a direct callback into the UI)
  v  Arduino loop task
src/main.cpp
  loop() -> pollCurrentScreen() [250 ms gate]
  -> adafruitio_manager.cpp::getCacheRevision()
  -> updateAlertIndicator()
  -> screens[state.currentScreen].poll()
  |
  v
src/screens/temperatures_screen.cpp
  refreshTemperaturesResults() -> updateTemperatureRows(false)
  -> adafruitio_manager.cpp::getFeedValue("pages.p01-temp", payload)
  |
  +--> src/screens/temperature_page_values.h
  |      parseTemperaturePageValues() -> ArduinoJson validation
  |      -> return parsed values to temperatures_screen.cpp
  |
  +--> include/cyd_helpers.h
  |      formatTemperatureValue() -> return display strings
  |
  v
src/screens/text_table_page.cpp
  refreshTextTablePageRows() [only changed rows]
  -> include/cyd_display.h::setBodyFont()
  -> gfx->fillRect() / setCursor() / print()
  |
  | gfx is the shared Arduino_ILI9341 object wired in src/cyd_globals.cpp
  | to Arduino_ESP32SPI; these are object definitions, not callback steps
  v
External Arduino GFX library: ILI9341 display driver + ESP32 SPI bus
  |
  v
CYD ILI9341 TFT pixels
```

Other page types branch after the same manager/cache/main-loop handoff:

```text
src/main.cpp::screens[current].poll()
  |
  +--> src/screens/aqi_screen.cpp::refreshAqiResults()
  |      -> adafruitio_manager.cpp::getFeedValue()
  |      -> src/screens/aqi_page_values.h::validateAqiPageValues()
  |      -> aqi_screen.cpp::drawAqiText() / gfx->fillRoundRect() / print()
  |      -> same shared gfx / ILI9341 / SPI / TFT
  |
  +--> src/screens/temperature_history_screen.cpp::refreshTemperatureHistoryResults()
         -> src/screens/snapshot_history_page.cpp::refreshSnapshotHistoryPage()
         -> adafruitio_manager.cpp::{getFeedValueRevision,getFeedUpdatedAt,getFeedValue}()
         -> src/screens/weather_snapshot_history.h::parseWeatherSnapshotHistory()
         -> src/screens/history_chart_page.cpp::refreshHistoryChartData()
         -> history_chart_page.cpp::refreshHistoryChartResults()
         -> src/widgets/line_chart_widget.cpp::drawLineChartSeriesWidget()
         -> line_chart_widget.cpp::drawChartWidget() -> drawSeries()
         -> gfx drawing primitives -> same shared ILI9341 / SPI / TFT
```

External library implementation files are outside this inventory. No application
sprite, canvas, or off-screen framebuffer is allocated by the scanned code.
