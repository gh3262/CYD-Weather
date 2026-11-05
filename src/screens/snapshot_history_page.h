#ifndef SNAPSHOT_HISTORY_PAGE_H
#define SNAPSHOT_HISTORY_PAGE_H

#include "screens/history_chart_page.h"
#include "screens/weather_snapshot_history.h"

void drawSnapshotHistoryPage(const HistoryChartPage &page, const char *feedKey,
                             const SnapshotHistoryField fields[]);
void refreshSnapshotHistoryPage(const HistoryChartPage &page);

#endif
