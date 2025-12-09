#pragma once
using namespace std::literals;

struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
};