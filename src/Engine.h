#pragma once

#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <unordered_map>
#include "Order.h"
#include "Types.h"

class Client;

extern std::atomic<int> totalTradesExecuted;

enum class ResponseStatus {
    SUCCESS,
    INVALID_ORDER,
    ORDER_NOT_FOUND,
    INSUFFICIENT_FUNDS,
    SYSTEM_ERROR
};

struct Response {
    ResponseStatus status;
    std::string reason;
    OrderId orderId;

    Response(ResponseStatus s, const std::string& r, OrderId id = OrderId(-1))
        : status(s), reason(r), orderId(id) {}
};

class Engine {
public:
    Engine();
    
    // Delete copy constructor and assignment operator
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    // Place an order
    Response placeOrder(OrderType type, Price price, Amount amount, std::shared_ptr<Client> client);
    
    // Cancel an order
    Response cancelOrder(OrderId orderId, std::shared_ptr<Client> client);
    
    // Destructor
    ~Engine();
    
private:
    // Order ID generator
    std::atomic<OrderId> nextOrderId;
    
    // Order books with smart pointers
    std::map<Price, std::queue<std::unique_ptr<Order>>, std::greater<Price>> buyOrders;
    std::map<Price, std::queue<std::unique_ptr<Order>>> sellOrders;
    
    // Use shared_ptr for order tracking
    std::unordered_map<OrderId, std::shared_ptr<Order>> orders;
    
    // Mutex for thread safety
    std::mutex orderBookMutex;
    
    // Helper method to match orders
    void matchOrders(std::unique_ptr<Order> newOrder);
    
    // Helper method to get the instance
    static Engine& getInstance();
    
    // Helper method to log order book state
    void logOrderBookState();
}; 