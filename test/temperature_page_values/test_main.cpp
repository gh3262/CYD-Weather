#include "screens/temperature_page_values.h"

#include <cstdio>
#include <cstdlib>

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
    TemperaturePageValues values = {};
    check(!parseTemperaturePageValues(
        "[71.775, 70.75, 68.4, 63.349999999999994, 78.1, 75.5]", values),
        "parse supplied six-value feed");
    const TemperaturePageValues expected = {71.775f, 70.75f, 68.4f, 63.35f, 78.1f, 75.5f};
    check(values == expected, "preserve all six row positions and float precision");
    check(!parseTemperaturePageValues(" [ -10, 0, 1, 2.5, 3e1, 32 ] ", values) &&
          values[0] == -10 && values[1] == 0 && values[4] == 30,
          "accept whitespace, integers, negative values, zero, and exponent notation");

    for (const char *invalid : {
        "", "not JSON", "[1,2,3", "[]", "[1,2,3,4,5]", "[1,2,3,4,5,6,7]",
        "{\"0\":1}", "\"[1,2,3,4,5,6]\"", "[1,2,3,4,5,null]",
        "[1,2,3,4,5,\"6\"]", "[1,2,3,4,5,true]", "[1,2,3,4,5,{}]",
        "[1,2,3,4,5,[]]", "[1,2,3,4,5,1e100]", "[1,2,3,4,5,NaN]",
        "[1,2,3,4,5,Infinity]"
    })
    {
        const TemperaturePageValues previous = values;
        check(parseTemperaturePageValues(invalid, values) != nullptr,
              "invalid snapshot supplies an explicit error");
        check(values == previous, "invalid snapshot never partially replaces values");
    }
    check(!parseTemperaturePageValues("[1,2,3,4,5,6]", values) && values[5] == 6,
          "valid update recovers after invalid snapshots");
    std::printf("PASS: %zu temperature-page checks\n", checks);
    return 0;
}
