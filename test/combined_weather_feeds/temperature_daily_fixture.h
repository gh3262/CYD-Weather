#ifndef TEMPERATURE_DAILY_FIXTURE_H
#define TEMPERATURE_DAILY_FIXTURE_H

static const char DAILY_TEMPERATURE_SNAPSHOT[] = R"json([
{"ts":"20261028","t2m_max":80.68,"t2m_min":50.69,"at_max":79.35,"at_min":48.87},
{"ts":"20261029","t2m_max":81.73,"t2m_min":51.57,"at_max":80.78,"at_min":48.77},
{"ts":"20261030","t2m_max":80.2,"t2m_min":51.05,"at_max":79.24,"at_min":48.5},
{"ts":"20261031","t2m_max":75.54,"t2m_min":61.7,"at_max":73.13,"at_min":60.68},
{"ts":"20261101","t2m_max":63.54,"t2m_min":59.08,"at_max":65.49,"at_min":56.31},
{"ts":"20261102","t2m_max":73.12,"t2m_min":62.55,"at_max":74.04,"at_min":64.09},
{"ts":"20261103","t2m_max":72.65,"t2m_min":65.58,"at_max":73.4,"at_min":64.22},
{"ts":"20261104","t2m_max":68.04,"t2m_min":51.3,"at_max":71.02,"at_min":51.0},
{"ts":"20261105","t2m_max":73.6,"t2m_min":48.4,"at_max":70.4,"at_min":46.51},
{"ts":"20261106","t2m_max":62.82,"t2m_min":48.51,"at_max":58.47,"at_min":44.59},
{"ts":"20261107","t2m_max":56.81,"t2m_min":38.88,"at_max":50.71,"at_min":35.0},
{"ts":"20261108","t2m_max":61.54,"t2m_min":33.34,"at_max":58.69,"at_min":29.59},
{"ts":"20261109","t2m_max":72.82,"t2m_min":35.69,"at_max":68.29,"at_min":32.04},
{"ts":"20261110","t2m_max":72.72,"t2m_min":37.41,"at_max":68.54,"at_min":33.27},
{"ts":"20261111","t2m_max":75.06,"t2m_min":47.42,"at_max":72.36,"at_min":45.04},
{"ts":"20261112","t2m_max":61.34,"t2m_min":39.15,"at_max":56.13,"at_min":32.04},
{"ts":"20261113","t2m_max":51.61,"t2m_min":32.36,"at_max":42.7,"at_min":25.24},
{"ts":"20261114","t2m_max":51.36,"t2m_min":33.32,"at_max":44.02,"at_min":26.3},
{"ts":"20261115","t2m_max":53.14,"t2m_min":35.25,"at_max":45.71,"at_min":27.98},
{"ts":"20261116","t2m_max":58.08,"t2m_min":39.42,"at_max":50.27,"at_min":32.41},
{"ts":"20261117","t2m_max":60.25,"t2m_min":38.27,"at_max":54.13,"at_min":31.99},
{"ts":"20261118","t2m_max":63.6,"t2m_min":40.68,"at_max":59.07,"at_min":35.06},
{"ts":"20261119","t2m_max":66.28,"t2m_min":42.58,"at_max":61.56,"at_min":36.98},
{"ts":"20261120","t2m_max":66.73,"t2m_min":48.33,"at_max":61.3,"at_min":43.12},
{"ts":"20261121","t2m_max":70.6,"t2m_min":51.05,"at_max":68.38,"at_min":46.66},
{"ts":"20261122","t2m_max":70.6,"t2m_min":50.05,"at_max":63.95,"at_min":43.43},
{"ts":"20261123","t2m_max":74.23,"t2m_min":55.2,"at_max":69.73,"at_min":51.74},
{"ts":"20261124","t2m_max":66.11,"t2m_min":39.81,"at_max":63.53,"at_min":31.17}
])json";

static const double DAILY_TEMPERATURE_MIN[] = {
    50.69, 51.57, 51.05, 61.7, 59.08, 62.55, 65.58, 51.3, 48.4, 48.51,
    38.88, 33.34, 35.69, 37.41, 47.42, 39.15, 32.36, 33.32, 35.25, 39.42,
    38.27, 40.68, 42.58, 48.33, 51.05, 50.05, 55.2, 39.81,
};
static const double DAILY_TEMPERATURE_MAX[] = {
    80.68, 81.73, 80.2, 75.54, 63.54, 73.12, 72.65, 68.04, 73.6, 62.82,
    56.81, 61.54, 72.82, 72.72, 75.06, 61.34, 51.61, 51.36, 53.14, 58.08,
    60.25, 63.6, 66.28, 66.73, 70.6, 70.6, 74.23, 66.11,
};
static const SnapshotHistoryField DAILY_TEMPERATURE_FIELDS[] = {
    {"t2m_min", HistoryAggregation::Minimum},
    {"t2m_max", HistoryAggregation::Maximum},
};

#endif
