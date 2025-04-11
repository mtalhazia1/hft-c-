#pragma once

#include "Types.h"
#include <string>
#include <mutex>

class Client {
public:
    Client(const std::string& name) : name(name) {}
    virtual ~Client() = default;
    
    void log(const std::string& message);
    void onOrderPlaced(OrderId orderId, Price price, Amount amount);
    void onOrderCanceled(OrderId orderId, int reasonCode);
    void onOrderTraded(OrderId orderId, Price price, Amount amount);
    const std::string& getName() const { return name; }

private:
    std::string name;
    static std::mutex coutMutex;
}; 