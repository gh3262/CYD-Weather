#ifndef TEMPERATURE_HOURLY_FIXTURE_H
#define TEMPERATURE_HOURLY_FIXTURE_H

static const double HOURLY_TEMPERATURES[24] = {
    39.42, 37.98, 37.26, 36.18, 35.73, 35.1, 34.38, 34.74,
    39.6, 44.55, 50.13, 53.19, 54.72, 56.07, 56.79, 57.15,
    55.98, 50.4, 46.98, 45.09, 43.65, 42.48, 41.76, 40.59,
};
static const double HOURLY_LOWER_BOUNDS[24] = {
    34.43, 33.59, 32.84, 32.15, 31.27, 30.71, 31.28, 31.21,
    35.95, 42.64, 47.05, 49.81, 51.79, 52.93, 53.49, 53.66,
    51.99, 47.36, 44.19, 40.65, 39.36, 37.98, 36.35, 35.11,
};
static const double HOURLY_UPPER_BOUNDS[24] = {
    50.82, 49.55, 48.88, 48.13, 48.01, 47.51, 47.36, 47.68,
    51.89, 56.18, 62.26, 66.42, 69.25, 71.89, 73.1, 72.52,
    71.62, 65.91, 62.11, 60.46, 58.18, 56.24, 53.88, 52.66,
};

static const char *HOURLY_TEMPERATURE_SNAPSHOT = R"json([
{"ts":"2026111600","temperature_2m":39.42,"lower_bound":34.43,"upper_bound":50.82,"avg_by_hour":42.62,"std_dev":8.2,"deviation":-3.2},
{"ts":"2026111601","temperature_2m":37.98,"lower_bound":33.59,"upper_bound":49.55},
{"ts":"2026111602","temperature_2m":37.26,"lower_bound":32.84,"upper_bound":48.88},
{"ts":"2026111603","temperature_2m":36.18,"lower_bound":32.15,"upper_bound":48.13},
{"ts":"2026111604","temperature_2m":35.73,"lower_bound":31.27,"upper_bound":48.01},
{"ts":"2026111605","temperature_2m":35.1,"lower_bound":30.71,"upper_bound":47.51},
{"ts":"2026111606","temperature_2m":34.38,"lower_bound":31.28,"upper_bound":47.36},
{"ts":"2026111607","temperature_2m":34.74,"lower_bound":31.21,"upper_bound":47.68},
{"ts":"2026111608","temperature_2m":39.6,"lower_bound":35.95,"upper_bound":51.89},
{"ts":"2026111609","temperature_2m":44.55,"lower_bound":42.64,"upper_bound":56.18},
{"ts":"2026111610","temperature_2m":50.13,"lower_bound":47.05,"upper_bound":62.26},
{"ts":"2026111611","temperature_2m":53.19,"lower_bound":49.81,"upper_bound":66.42},
{"ts":"2026111612","temperature_2m":54.72,"lower_bound":51.79,"upper_bound":69.25},
{"ts":"2026111613","temperature_2m":56.07,"lower_bound":52.93,"upper_bound":71.89},
{"ts":"2026111614","temperature_2m":56.79,"lower_bound":53.49,"upper_bound":73.1},
{"ts":"2026111615","temperature_2m":57.15,"lower_bound":53.66,"upper_bound":72.52},
{"ts":"2026111616","temperature_2m":55.98,"lower_bound":51.99,"upper_bound":71.62},
{"ts":"2026111617","temperature_2m":50.4,"lower_bound":47.36,"upper_bound":65.91},
{"ts":"2026111618","temperature_2m":46.98,"lower_bound":44.19,"upper_bound":62.11},
{"ts":"2026111619","temperature_2m":45.09,"lower_bound":40.65,"upper_bound":60.46},
{"ts":"2026111620","temperature_2m":43.65,"lower_bound":39.36,"upper_bound":58.18},
{"ts":"2026111621","temperature_2m":42.48,"lower_bound":37.98,"upper_bound":56.24},
{"ts":"2026111622","temperature_2m":41.76,"lower_bound":36.35,"upper_bound":53.88},
{"ts":"2026111623","temperature_2m":40.59,"lower_bound":35.11,"upper_bound":52.66}
])json";

#endif
