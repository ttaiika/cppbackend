#pragma once

#include <chrono>

class ApplicationListener {
public:
    virtual ~ApplicationListener() = default;
    virtual void OnTick(std::chrono::milliseconds delta) = 0;
}; 