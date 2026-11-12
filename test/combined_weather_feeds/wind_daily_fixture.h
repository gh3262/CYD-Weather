#ifndef WIND_DAILY_FIXTURE_H
#define WIND_DAILY_FIXTURE_H

static const char DAILY_WIND_SNAPSHOT[] = R"json([
{"ts":"20261028","wind_speed_10m_mean":3.74,"wind_gusts_10m_max":10.29,"wind_direction_10m_dominant":118.35},
{"ts":"20261029","wind_speed_10m_mean":3.34,"wind_gusts_10m_max":10.96,"wind_direction_10m_dominant":129.93},
{"ts":"20261030","wind_speed_10m_mean":4.06,"wind_gusts_10m_max":13.2,"wind_direction_10m_dominant":84.8},
{"ts":"20261031","wind_speed_10m_mean":6.07,"wind_gusts_10m_max":22.15,"wind_direction_10m_dominant":117.92},
{"ts":"20261101","wind_speed_10m_mean":8.18,"wind_gusts_10m_max":30.65,"wind_direction_10m_dominant":123.39},
{"ts":"20261102","wind_speed_10m_mean":8.72,"wind_gusts_10m_max":26.84,"wind_direction_10m_dominant":148.78},
{"ts":"20261103","wind_speed_10m_mean":11.64,"wind_gusts_10m_max":34.67,"wind_direction_10m_dominant":157.17},
{"ts":"20261104","wind_speed_10m_mean":5.94,"wind_gusts_10m_max":23.71,"wind_direction_10m_dominant":263.08},
{"ts":"20261105","wind_speed_10m_mean":2.5,"wind_gusts_10m_max":7.61,"wind_direction_10m_dominant":174.43},
{"ts":"20261106","wind_speed_10m_mean":7.78,"wind_gusts_10m_max":20.8,"wind_direction_10m_dominant":12.79},
{"ts":"20261107","wind_speed_10m_mean":5.61,"wind_gusts_10m_max":14.54,"wind_direction_10m_dominant":19.69},
{"ts":"20261108","wind_speed_10m_mean":2.23,"wind_gusts_10m_max":7.38,"wind_direction_10m_dominant":351.47},
{"ts":"20261109","wind_speed_10m_mean":2.51,"wind_gusts_10m_max":7.16,"wind_direction_10m_dominant":237.21},
{"ts":"20261110","wind_speed_10m_mean":3.23,"wind_gusts_10m_max":8.5,"wind_direction_10m_dominant":210.2},
{"ts":"20261111","wind_speed_10m_mean":6.74,"wind_gusts_10m_max":24.38,"wind_direction_10m_dominant":237.37},
{"ts":"20261112","wind_speed_10m_mean":8.06,"wind_gusts_10m_max":32.66,"wind_direction_10m_dominant":339.42},
{"ts":"20261113","wind_speed_10m_mean":8.06,"wind_gusts_10m_max":23.49,"wind_direction_10m_dominant":344.89},
{"ts":"20261114","wind_speed_10m_mean":5.36,"wind_gusts_10m_max":16.78,"wind_direction_10m_dominant":33.86},
{"ts":"20261115","wind_speed_10m_mean":6.09,"wind_gusts_10m_max":15.44,"wind_direction_10m_dominant":65.28},
{"ts":"20261116","wind_speed_10m_mean":7.48,"wind_gusts_10m_max":15.88,"wind_direction_10m_dominant":45.36},
{"ts":"20261117","wind_speed_10m_mean":5.24,"wind_gusts_10m_max":12.97,"wind_direction_10m_dominant":64.36},
{"ts":"20261118","wind_speed_10m_mean":4.17,"wind_gusts_10m_max":6.93,"wind_direction_10m_dominant":97.44},
{"ts":"20261119","wind_speed_10m_mean":3.41,"wind_gusts_10m_max":8.05,"wind_direction_10m_dominant":113.42},
{"ts":"20261120","wind_speed_10m_mean":2.02,"wind_gusts_10m_max":13.2,"wind_direction_10m_dominant":159.0},
{"ts":"20261121","wind_speed_10m_mean":2.36,"wind_gusts_10m_max":18.34,"wind_direction_10m_dominant":136.97},
{"ts":"20261122","wind_speed_10m_mean":8.1,"wind_gusts_10m_max":24.83,"wind_direction_10m_dominant":185.6},
{"ts":"20261123","wind_speed_10m_mean":7.92,"wind_gusts_10m_max":20.8,"wind_direction_10m_dominant":167.38},
{"ts":"20261124","wind_speed_10m_mean":14.12,"wind_gusts_10m_max":21.7,"wind_direction_10m_dominant":218.52}
])json";

static const double DAILY_WIND_SPEED[] = {
    3.74, 3.34, 4.06, 6.07, 8.18, 8.72, 11.64, 5.94, 2.5, 7.78, 5.61,
    2.23, 2.51, 3.23, 6.74, 8.06, 8.06, 5.36, 6.09, 7.48, 5.24, 4.17,
    3.41, 2.02, 2.36, 8.1, 7.92, 14.12,
};
static const double DAILY_WIND_GUST[] = {
    10.29, 10.96, 13.2, 22.15, 30.65, 26.84, 34.67, 23.71, 7.61, 20.8,
    14.54, 7.38, 7.16, 8.5, 24.38, 32.66, 23.49, 16.78, 15.44, 15.88,
    12.97, 6.93, 8.05, 13.2, 18.34, 24.83, 20.8, 21.7,
};
static const SnapshotHistoryField DAILY_WIND_FIELDS[] = {
    {"wind_speed_10m_mean", HistoryAggregation::Average},
    {"wind_gusts_10m_max", HistoryAggregation::Maximum},
};

#endif
