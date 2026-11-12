#ifndef CLOUDS_FIXTURE_H
#define CLOUDS_FIXTURE_H

static const char CLOUDS_SNAPSHOT[] = R"json([
{"ts":"20261028","dld":10.86,"ssd":9.65,"pct_ss":88.86,"cc_mean":5.0,"cc_max":50.0,"cc_min":0.0},
{"ts":"20261029","pct_ss":88.98,"cc_mean":12},
{"ts":"20261030","pct_ss":86.62,"cc_mean":15},
{"ts":"20261031","pct_ss":9.29,"cc_mean":90},
{"ts":"20261101","pct_ss":0,"cc_mean":100},
{"ts":"20261102","pct_ss":9.34,"cc_mean":80},
{"ts":"20261103","pct_ss":48.71,"cc_mean":55},
{"ts":"20261104","pct_ss":40.81,"cc_mean":60},
{"ts":"20261105","pct_ss":85.51,"cc_mean":20},
{"ts":"20261106","pct_ss":76.9,"cc_mean":25},
{"ts":"20261107","pct_ss":79.72,"cc_mean":22},
{"ts":"20261108","pct_ss":76.08,"cc_mean":24},
{"ts":"20261109","pct_ss":86.84,"cc_mean":16},
{"ts":"20261110","pct_ss":76.51,"cc_mean":23},
{"ts":"20261111","pct_ss":76.72,"cc_mean":22.62},
{"ts":"20261112","pct_ss":72.07,"cc_mean":30},
{"ts":"20261113","pct_ss":96.2,"cc_mean":4},
{"ts":"20261114","pct_ss":95.6,"cc_mean":5},
{"ts":"20261115","pct_ss":94.13,"cc_mean":7},
{"ts":"20261116","pct_ss":95.23,"cc_mean":6},
{"ts":"20261117","pct_ss":95.45,"cc_mean":8},
{"ts":"20261118","pct_ss":95.51,"cc_mean":9},
{"ts":"20261119","pct_ss":95.44,"cc_mean":10},
{"ts":"20261120","pct_ss":94.01,"cc_mean":92},
{"ts":"20261121","pct_ss":91.7,"cc_mean":90},
{"ts":"20261122","pct_ss":91.09,"cc_mean":80},
{"ts":"20261123","pct_ss":36.88,"cc_mean":75},
{"ts":"20261124","pct_ss":96.4,"cc_mean":0}
])json";

static const double CLOUDS_SUNSHINE[] = {
    88.86, 88.98, 86.62, 9.29, 0, 9.34, 48.71, 40.81, 85.51, 76.9,
    79.72, 76.08, 86.84, 76.51, 76.72, 72.07, 96.2, 95.6, 94.13, 95.23,
    95.45, 95.51, 95.44, 94.01, 91.7, 91.09, 36.88, 96.4,
};
static const double CLOUDS_MEAN[] = {
    5, 12, 15, 90, 100, 80, 55, 60, 20, 25, 22, 24, 16, 23,
    22.62, 30, 4, 5, 7, 6, 8, 9, 10, 92, 90, 80, 75, 0,
};

#endif
