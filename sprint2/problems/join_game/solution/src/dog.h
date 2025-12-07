#pragma once
#include <string>
#include <cstdint>

class Dog {
public:
    Dog(uint32_t id, const std::string& name)
        : id_(id)
        , name_(name) {
    }

    uint32_t GetId() const {
        return id_;
    }

    std::string GetName() const {
        return name_;
    }

private:
    uint32_t id_;
    std::string name_;
};