#pragma once
#include <cstdint>

enum class OrderType {
    BUY,
    SELL
};

// Strong types for better type safety
struct OrderId {
    int32_t value;
    explicit OrderId(int32_t v) : value(v) {}
    operator int32_t() const { return value; }
};

struct Price {
    int32_t value;
    explicit Price(int32_t v) : value(v) {}
    operator int32_t() const { return value; }
};

struct Amount {
    int32_t value;
    explicit Amount(int32_t v) : value(v) {}
    operator int32_t() const { return value; }
}; 