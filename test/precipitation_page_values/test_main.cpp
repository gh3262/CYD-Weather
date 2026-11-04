#include "screens/precipitation_page_values.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static size_t checks = 0;

static void check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    PrecipitationPageValues values = {};
    check(!parsePrecipitationPageValues(
              "[\"91\", \"rain\", \"0.08\", \"56\", \"rain\", \"0.03\", "
              "\"26\", \"rain\", \"0.00\", \"0\", \"clear\", \"0.00\", "
              "\"0\", \"clear\", \"0.00\", \"0\", \"clear\", \"0.00\", "
              "\"0\", \"clear\", \"0.00\"]",
              values),
          "parse the supplied page feed payload");
    check(std::strcmp(values[0].data(), "91") == 0 &&
              std::strcmp(values[1].data(), "rain") == 0 &&
              std::strcmp(values[2].data(), "0.08") == 0 &&
              std::strcmp(values[18].data(), "0") == 0 &&
              std::strcmp(values[19].data(), "clear") == 0 &&
              std::strcmp(values[20].data(), "0.00") == 0,
          "preserve supplied values and row-major order");
    check(!parsePrecipitationPageValues(
              "[0,\"\",0,1,\"Rain\",0.01,2,\"Snow\",0.02,3,\"Ice\",0.03,"
              "4,\"Fog\",0.04,5,\"Sleet\",0.05,6,\"Rain\",0.06]",
              values) &&
              std::strcmp(values[0].data(), "0") == 0 &&
              std::strcmp(values[20].data(), "0.06") == 0,
          "accept empty types, zero, and fractional values");
    check(!parsePrecipitationPageValues(
              "[\"10\",\"Rain\",\"0.1\",\"20\",\"Snow\",\"0.25\","
              "\"30\",\"Sleet\",\"0\",\"40\",\"Ice\",\"1\","
              "\"50\",\"Fog\",\"2.5\",\"60\",\"Heavy Rain\",\"3.25\","
              "\"70\",\"\",\"0\"]",
              values) &&
              std::strcmp(values[0].data(), "10") == 0 &&
              std::strcmp(values[2].data(), "0.1") == 0 &&
              std::strcmp(values[20].data(), "0") == 0,
          "accept numeric values encoded as strings by the page feed");
    check(!parsePrecipitationPageValues(
              "[\" 10.5 \",\"Rain\",\"+0.1\",\"20\",\"Snow\",\"2e-1\","
              "\"30\",\"Sleet\",\"0\",\"40\",\"Ice\",\"1\","
              "\"50\",\"Fog\",\"2.5\",\"60\",\"Heavy Rain\",\"3.25\","
              "\"70\",\"\",\"0\"]",
              values) &&
              std::strcmp(values[0].data(), "10.5") == 0 &&
              std::strcmp(values[2].data(), "+0.1") == 0,
          "trim and preserve valid numeric strings");

    for (const char *invalid : {
             "", "not JSON", "[1,2,3", "[]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1]",
             "{\"0\":1}", "\"[1]\"",
             "[\"Rainy\",\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0]",
             "[1,2,0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",null,1]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",1e100,1]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",NaN,1]",
             "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
             "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",Infinity,1]"})
    {
        const PrecipitationPageValues previous = values;
        check(parsePrecipitationPageValues(invalid, values) != nullptr,
              "invalid snapshot supplies an explicit error");
        check(values == previous,
              "invalid snapshot never partially replaces values");
    }
    check(!parsePrecipitationPageValues(
              "[1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,"
              "1,\"Rain\",0,1,\"Rain\",0,1,\"Rain\",0,1]",
              values) &&
              std::strcmp(values[19].data(), "0") == 0,
          "valid update recovers after invalid snapshots");
    std::printf("PASS: %zu precipitation-page checks\n", checks);
    return 0;
}
