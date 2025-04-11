#pragma once

#include <chrono>
#include <memory>
#include "Types.h"

// Forward declaration
class Client;

// Add hash function for OrderId
namespace std {
    template<>
    struct hash<OrderId> {
        size_t operator()(const OrderId& id) const noexcept {
            return static_cast<size_t>(id.value);
        }
    };
}

struct Order {
    OrderId orderId;
    OrderType type;
    Price price;
    Amount amount;
    Amount remainingAmount;
    std::shared_ptr<Client> client;
    std::chrono::system_clock::time_point timestamp;

    Order(OrderId id, OrderType t, Price p, Amount a, std::shared_ptr<Client> c) 
        : orderId(id), type(t), price(p), amount(a), remainingAmount(a), client(c),
          timestamp(std::chrono::system_clock::now()) {}
}; 