#ifndef CLOUDS_FIXTURE_H
#define CLOUDS_FIXTURE_H

static const char CLOUDS_SNAPSHOT[] = R"json([
{"ts":"20261028","dld":10.86,"ssd":9.65,"pct_ss":88.86,"cc_mean":5.0,"cc_max":50.0,"cc_min":0.0},
{"ts":"20261029","pct_ss":88.98,"cc_max":85,"cc_min":0},
{"ts":"20261030","pct_ss":86.62,"cc_max":99,"cc_min":0},
{"ts":"20261031","pct_ss":9.29,"cc_max":100,"cc_min":66},
{"ts":"20261101","pct_ss":0,"cc_max":100,"cc_min":99},
{"ts":"20261102","pct_ss":9.34,"cc_max":100,"cc_min":0},
{"ts":"20261103","pct_ss":48.71,"cc_max":100,"cc_min":1},
{"ts":"20261104","pct_ss":40.81,"cc_max":100,"cc_min":0},
{"ts":"20261105","pct_ss":85.51,"cc_max":100,"cc_min":0},
{"ts":"20261106","pct_ss":76.9,"cc_max":100,"cc_min":0},
{"ts":"20261107","pct_ss":79.72,"cc_max":99,"cc_min":0},
{"ts":"20261108","pct_ss":76.08,"cc_max":100,"cc_min":1},
{"ts":"20261109","pct_ss":86.84,"cc_max":100,"cc_min":0},
{"ts":"20261110","pct_ss":76.51,"cc_max":100,"cc_min":0},
{"ts":"20261111","pct_ss":76.72,"cc_max":80,"cc_min":0},
{"ts":"20261112","pct_ss":72.07,"cc_max":100,"cc_min":0},
{"ts":"20261113","pct_ss":96.2,"cc_max":100,"cc_min":0},
{"ts":"20261114","pct_ss":95.6,"cc_max":100,"cc_min":0},
{"ts":"20261115","pct_ss":94.13,"cc_max":100,"cc_min":6},
{"ts":"20261116","pct_ss":95.23,"cc_max":100,"cc_min":0},
{"ts":"20261117","pct_ss":95.45,"cc_max":81,"cc_min":0},
{"ts":"20261118","pct_ss":95.51,"cc_max":85,"cc_min":0},
{"ts":"20261119","pct_ss":95.44,"cc_max":100,"cc_min":0},
{"ts":"20261120","pct_ss":94.01,"cc_max":100,"cc_min":86},
{"ts":"20261121","pct_ss":91.7,"cc_max":100,"cc_min":81},
{"ts":"20261122","pct_ss":91.09,"cc_max":100,"cc_min":67},
{"ts":"20261123","pct_ss":36.88,"cc_max":100,"cc_min":44},
{"ts":"20261124","pct_ss":96.4,"cc_max":60,"cc_min":0}
])json";

static const double CLOUDS_SUNSHINE[] = {
    88.86, 88.98, 86.62, 9.29, 0, 9.34, 48.71, 40.81, 85.51, 76.9,
    79.72, 76.08, 86.84, 76.51, 76.72, 72.07, 96.2, 95.6, 94.13, 95.23,
    95.45, 95.51, 95.44, 94.01, 91.7, 91.09, 36.88, 96.4,
};
static const double CLOUDS_MIN[] = {
    0, 0, 0, 66, 99, 0, 1, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 6, 0, 0, 0, 0, 86, 81, 67, 44, 0,
};
static const double CLOUDS_MAX[] = {
    50, 85, 99, 100, 100, 100, 100, 100, 100, 100, 99, 100, 100, 100,
    80, 100, 100, 100, 100, 100, 81, 85, 100, 100, 100, 100, 100, 60,
};

#endif
