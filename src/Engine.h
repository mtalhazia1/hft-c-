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
    // Constants for order ID limits
    static constexpr OrderId MAX_ORDER_ID = OrderId(std::numeric_limits<int>::max());
    static constexpr OrderId MIN_ORDER_ID = OrderId(0);
    
    // Constructor with dependency injection
    Engine();
    
    // Delete copy constructor and assignment operator
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    // Place an order
    Response placeOrder(OrderType type, Price price, Amount amount, std::shared_ptr<Client> client);
    
    // Cancel an order
    Response cancelOrder(OrderId orderId, std::shared_ptr<Client> client);
    
    // Get total trades executed
    int getTotalTradesExecuted() const { return totalTradesExecuted.load(); }
    
    // Destructor
    ~Engine();
    
private:
    // Order ID generator with proper atomic operations
    std::atomic<OrderId> nextOrderId;
    
    // Total trades executed counter
    std::atomic<int> totalTradesExecuted;
    
    // Order books with shared pointers
    std::map<Price, std::queue<std::shared_ptr<Order>>, std::greater<Price>> buyOrders;
    std::map<Price, std::queue<std::shared_ptr<Order>>> sellOrders;
    
    // Use shared_ptr for order tracking
    std::unordered_map<OrderId, std::shared_ptr<Order>> orders;
    
    // Mutexes for thread safety
    std::mutex orderMapMutex;  // For orders lookup map
    std::mutex buyOrdersMutex; // For buy order book
    std::mutex sellOrdersMutex; // For sell order book
    
    // Helper method to match orders
    void matchOrders(std::shared_ptr<Order> newOrder);
    
    // Helper method to log order book state
    void logOrderBookState();
    
    // Helper method to generate next order ID
    OrderId generateNextOrderId();
    
    // Helper methods for order book operations
    bool removeOrderFromBook(std::shared_ptr<Order> order);
    void addOrderToBook(std::shared_ptr<Order> order);
    
    // Helper method to execute a trade between two orders
    void executeTrade(std::shared_ptr<Order> buyOrder, std::shared_ptr<Order> sellOrder, Amount tradeAmount);
}; 