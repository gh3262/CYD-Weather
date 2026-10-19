#ifndef HISTORY_TEST_HTTP_H
#define HISTORY_TEST_HTTP_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <deque>
#include <functional>
#include <map>
#include <sstream>
#include <vector>

static const int HTTP_CODE_OK = 200;
struct TestHttpResponse
{
    int status;
    std::string body;
    std::map<std::string, std::string> headers;
};
extern std::deque<TestHttpResponse> testResponses;
extern std::vector<std::string> testUrls;
extern std::function<void()> testOnGet;

struct HTTPClient
{
    TestHttpResponse response;
    std::istringstream stream;
    void setConnectTimeout(uint32_t) {}
    void setTimeout(uint32_t) {}
    void useHTTP10(bool) {}
    bool begin(WiFiClientSecure &, const String &url)
    {
        testUrls.push_back(url.c_str());
        return true;
    }
    void collectHeaders(const char **, size_t) {}
    void addHeader(const char *, const char *) {}
    int GET()
    {
        if (testOnGet)
        {
            const std::function<void()> callback = testOnGet;
            testOnGet = nullptr;
            callback();
        }
        if (testResponses.empty())
            std::abort();
        response = testResponses.front();
        testResponses.pop_front();
        stream.str(response.body);
        return response.status;
    }
    String header(const char *name)
    {
        const auto found = response.headers.find(name);
        return found == response.headers.end() ? String() : String(found->second);
    }
    std::istringstream &getStream() { return stream; }
    void end() {}
};

#endif
