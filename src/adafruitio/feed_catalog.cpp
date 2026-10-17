#include "adafruitio_manager.h"

const AdafruitIOFeedDefinition AIO_FEEDS[] = {
    {"big-alert", "Full Alert", "Info Text", AdafruitIOValueType::LongText, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"headline-alert", "Headline Alert", "Info Text", AdafruitIOValueType::LongText, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"weather-alerts", "Weather Alert Summary", "Info Text", AdafruitIOValueType::LongText, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"nbr-alerts", "Number of Active Alerts", "Info Text", AdafruitIOValueType::Integer, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentcloudcover", "Current Cloud Cover", "Clouds", AdafruitIOValueType::Integer, "%", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentconditioncode", "Current Conditions", "Condition", AdafruitIOValueType::Text, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"currenthumidity", "Current Humidity", "Humidity", AdafruitIOValueType::Integer, "%", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentprecipitationintensity", "Current Precipitation Intensity", "Precipitation", AdafruitIOValueType::Float, "in/hr", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentpressure", "Current Barometer", "Barometer", AdafruitIOValueType::Float, "hPa", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentpressuretrend", "Barometer Trend", "Barometer", AdafruitIOValueType::Text, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"currenttemperature", "Current Temperature (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"currenttemperatureapparent", "Current Apparent Temperature (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"currenttemperaturedewpoint", "Current Dew Point (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentuv", "Current UV Index", "Condition", AdafruitIOValueType::Integer, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentwinddirection", "Current Wind Direction", "Wind", AdafruitIOValueType::Text, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentwindgust", "Current Wind Gust (mph)", "Wind", AdafruitIOValueType::Integer, "mph", false, AdafruitIORefreshPolicy::OnDemand},
    {"currentwindspeed", "Current Wind Speed (mph)", "Wind", AdafruitIOValueType::Integer, "mph", false, AdafruitIORefreshPolicy::OnDemand},
    {"feed-avg", "Current Average Temperature (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::Periodic},
    {"wapi.feels-like", "Current Feels Like Temperature (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"wapi.desc", "Current Conditions WAPI", "Condition", AdafruitIOValueType::Text, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"wapi.heat-index", "Current Heat Index (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"wapi.wind-chill", "Current Wind Chill (F)", "Temperature", AdafruitIOValueType::Float, "°F", false, AdafruitIORefreshPolicy::OnDemand},
    {"om-desc", "Current Weather Description", "Condition", AdafruitIOValueType::Text, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"om-current", "Current Long Weather Description", "Info Text", AdafruitIOValueType::Text, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"currentwinddegrees", "Current Wind Bearing", "Wind", AdafruitIOValueType::Integer, "", false, AdafruitIORefreshPolicy::OnDemand},
    {"nws.00", "Current Forecast", "Info Text", AdafruitIOValueType::Text, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"nws.01", "Next Forecast", "Info Text", AdafruitIOValueType::Text, "", true, AdafruitIORefreshPolicy::OnDemand},
    {"nws.02", "Following Forecast", "Info Text", AdafruitIOValueType::Text, "", true, AdafruitIORefreshPolicy::OnDemand},
};

const size_t AIO_FEED_COUNT = sizeof(AIO_FEEDS) / sizeof(AIO_FEEDS[0]);

static_assert(AIO_FEED_COUNT == AIO_FEED_CAPACITY,
              "Update AIO_FEED_CAPACITY when extending the feed catalog");
