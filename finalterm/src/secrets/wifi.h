#include <cstdint>
namespace WiFiSecrets
{
    const char *ssid = "TP-Link_7588";
    const char *pass = "53491855";
    const char *echo_topic = "esp32/echo_test";
    unsigned int publish_count = 0;
    uint16_t keepAlive = 15;    // seconds (default is 15)
    uint16_t socketTimeout = 5; // seconds (default is 15)
}