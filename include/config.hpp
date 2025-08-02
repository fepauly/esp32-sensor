#pragma once

#include <string_view>

#ifndef WIFI_SSID
    #error "WIFI_SSID not defined - set via build flag"
#endif
#ifndef WIFI_PASS
    #error "WIFI_PASS not defined - set via build flag"
#endif

namespace cfg
{
    inline constexpr std::string_view kWifiSsid{WIFI_SSID};
    inline constexpr std::string_view kWifiPass{WIFI_PASS};
} // namespace cfg
