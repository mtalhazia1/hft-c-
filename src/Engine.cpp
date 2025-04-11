#include "Engine.h"
#include "Client.h"
#include "Order.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <queue>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include <limits>

// Remove extern declaration
// extern std::atomic<int> totalTradesExecuted;

// Remove singleton instance
// std::unique_ptr<Engine> Engine::instance = nullptr;
// std::mutex Engine::instanceMutex;

Engine::Engine() : nextOrderId(OrderId(0)), totalTradesExecuted(0) {
    std::cout << "Trading Engine started" << std::endl;
}

OrderId Engine::generateNextOrderId() {
    OrderId currentId = nextOrderId.load(std::memory_order_acquire);
    OrderId newId = OrderId(currentId.value + 1);
    
    do {
        // Check for overflow
        if (newId.value <= currentId.value) {
            std::cerr << "Warning: Order ID overflow detected, resetting to " << MIN_ORDER_ID.value << std::endl;
            newId = MIN_ORDER_ID;
        }
    } while (!nextOrderId.compare_exchange_weak(currentId, newId, 
                                              std::memory_order_release,
                                              std::memory_order_relaxed));
    
    return currentId;
}

// Helper method to add order to the appropriate order book
void Engine::addOrderToBook(std::shared_ptr<Order> order) {
    if (order->type == OrderType::BUY) {
        std::lock_guard<std::mutex> lock(buyOrdersMutex);
        buyOrders[order->price].push(order);
    } else {
        std::lock_guard<std::mutex> lock(sellOrdersMutex);
        sellOrders[order->price].push(order);
    }
}

// Helper method to remove order from the appropriate order book
bool Engine::removeOrderFromBook(std::shared_ptr<Order> order) {
    bool found = false;
    
    if (order->type == OrderType::BUY) {
        std::lock_guard<std::mutex> lock(buyOrdersMutex);
        auto& queue = buyOrders[order->price];
        size_t size = queue.size();
        
        for (size_t i = 0; i < size; ++i) {
            auto front = queue.front();
            queue.pop();
            if (front->orderId != order->orderId) {
                queue.push(front);
            } else {
                found = true;
            }
        }
        
        if (queue.empty()) {
            buyOrders.erase(order->price);
        }
    } else {
        std::lock_guard<std::mutex> lock(sellOrdersMutex);
        auto& queue = sellOrders[order->price];
        size_t size = queue.size();
        
        for (size_t i = 0; i < size; ++i) {
            auto front = queue.front();
            queue.pop();
            if (front->orderId != order->orderId) {
                queue.push(front);
            } else {
                found = true;
            }
        }
        
        if (queue.empty()) {
            sellOrders.erase(order->price);
        }
    }
    
    return found;
}

Response Engine::placeOrder(OrderType type, Price price, Amount amount, std::shared_ptr<Client> client) {
    if (!client) {
        return Response(ResponseStatus::INVALID_ORDER, "Invalid client");
    }

    if (amount.value <= 0 || price.value <= 0) {
        return Response(ResponseStatus::INVALID_ORDER, "Invalid amount or price");
    }

    // Generate new order ID using atomic operations
    OrderId orderId = generateNextOrderId();
    auto order = std::make_shared<Order>(orderId, type, price, amount, client);

    auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);

    std::cout << "\n[Time: " << duration.count() << "μs] New order received: " 
              << (type == OrderType::BUY ? "BUY" : "SELL") 
              << " OrderId: " << orderId.value 
              << " Price: " << price.value 
              << " Amount: " << amount.value << std::endl;

    // Store order in lookup map
    {
        std::lock_guard<std::mutex> lock(orderMapMutex);
        orders[orderId] = order;
    }

    // Try to match orders first
    matchOrders(order);

    return Response(ResponseStatus::SUCCESS, "Order placed successfully", orderId);
}

Response Engine::cancelOrder(OrderId orderId, std::shared_ptr<Client> client) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!client) {
        return Response(ResponseStatus::INVALID_ORDER, "Invalid client");
    }
    
    // First find the order in the lookup map
    std::shared_ptr<Order> order;
    {
        std::lock_guard<std::mutex> lock(orderMapMutex);
        auto it = orders.find(orderId);
        if (it == orders.end()) {
            std::cout << "Order not found" << std::endl;
            return Response(ResponseStatus::ORDER_NOT_FOUND, "Order not found");
        }
        order = it->second;
    }
    
    if (order->client != client) {
        std::cout << "Order does not belong to client" << std::endl;
        return Response(ResponseStatus::INVALID_ORDER, "Order does not belong to client");
    }
    
    // Remove from order book
    bool found = removeOrderFromBook(order);
    
    if (found) {
        // Remove from lookup map
        {
            std::lock_guard<std::mutex> lock(orderMapMutex);
            orders.erase(orderId);
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
        
        std::cout << "\n[Time: " << duration.count() << "μs] Cancel request received for OrderId: " 
                  << orderId.value << std::endl;
        
        std::cout << "Order cancelled. Current state:" << std::endl;
        logOrderBookState();
        
        return Response(ResponseStatus::SUCCESS, "Order cancelled successfully");
    }
    
    return Response(ResponseStatus::ORDER_NOT_FOUND, "Order not found in order book");
}

void Engine::matchOrders(std::shared_ptr<Order> newOrder) {
    bool orderAddedToBook = false;
    
    try {
        if (newOrder->type == OrderType::BUY) {
            // For buy orders, look at sell orders
            std::lock_guard<std::mutex> sellLock(sellOrdersMutex);
            for (auto it = sellOrders.begin(); it != sellOrders.end() && newOrder->remainingAmount.value > 0;) {
                auto& [price, queue] = *it;
                
                if (price.value > newOrder->price.value) {
                    break; // No more matching prices
                }
                
                while (!queue.empty() && newOrder->remainingAmount.value > 0) {
                    auto sellOrder = queue.front();
                    queue.pop();
                    
                    Amount tradeAmount = std::min(newOrder->remainingAmount, sellOrder->remainingAmount);
                    
                    // Execute trade
                    executeTrade(newOrder, sellOrder, tradeAmount);
                    
                    if (sellOrder->remainingAmount.value > 0) {
                        queue.push(sellOrder);
                        break;
                    }
                }
                
                if (queue.empty()) {
                    it = sellOrders.erase(it);
                } else {
                    ++it;
                }
            }
        } else {
            // For sell orders, look at buy orders
            std::lock_guard<std::mutex> buyLock(buyOrdersMutex);
            for (auto it = buyOrders.begin(); it != buyOrders.end() && newOrder->remainingAmount.value > 0;) {
                auto& [price, queue] = *it;
                
                if (price.value < newOrder->price.value) {
                    break; // No more matching prices
                }
                
                while (!queue.empty() && newOrder->remainingAmount.value > 0) {
                    auto buyOrder = queue.front();
                    queue.pop();
                    
                    Amount tradeAmount = std::min(newOrder->remainingAmount, buyOrder->remainingAmount);
                    
                    // Execute trade
                    executeTrade(buyOrder, newOrder, tradeAmount);
                    
                    if (buyOrder->remainingAmount.value > 0) {
                        queue.push(buyOrder);
                        break;
                    }
                }
                
                if (queue.empty()) {
                    it = buyOrders.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // If order wasn't fully matched, add remaining to book
        if (newOrder->remainingAmount.value > 0) {
            addOrderToBook(newOrder);
            orderAddedToBook = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in matchOrders: " << e.what() << std::endl;
        if (!orderAddedToBook && newOrder->remainingAmount.value > 0) {
            addOrderToBook(newOrder);
        }
    }
}

void Engine::executeTrade(std::shared_ptr<Order> buyOrder, std::shared_ptr<Order> sellOrder, Amount tradeAmount) {
    // Calculate trade price (use the price from the order that was in the book)
    Price tradePrice = (buyOrder->type == OrderType::BUY) ? sellOrder->price : buyOrder->price;
    
    // Update remaining amounts
    buyOrder->remainingAmount.value -= tradeAmount.value;
    sellOrder->remainingAmount.value -= tradeAmount.value;
    
    // Notify clients about the trade
    buyOrder->client->onOrderTraded(buyOrder->orderId, tradePrice, tradeAmount);
    sellOrder->client->onOrderTraded(sellOrder->orderId, tradePrice, tradeAmount);
    
    // Increment total trades counter
    totalTradesExecuted++;
    
    std::cout << "Match found! Trade executed:" << std::endl;
    std::cout << "Buy OrderId: " << buyOrder->orderId.value 
              << " Sell OrderId: " << sellOrder->orderId.value
              << " Price: " << tradePrice.value
              << " Amount: " << tradeAmount.value << std::endl;
}

void Engine::logOrderBookState() {
    std::cout << "\nCurrent Order Book State:" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    {
        std::lock_guard<std::mutex> buyLock(buyOrdersMutex);
        std::cout << "Buy Orders:" << std::endl;
        for (const auto& [price, queue] : buyOrders) {
            std::cout << "Price: " << price.value << " - Orders: " << queue.size() << std::endl;
        }
    }
    
    {
        std::lock_guard<std::mutex> sellLock(sellOrdersMutex);
        std::cout << "Sell Orders:" << std::endl;
        for (const auto& [price, queue] : sellOrders) {
            std::cout << "Price: " << price.value << " - Orders: " << queue.size() << std::endl;
        }
    }
    
    std::cout << "------------------------" << std::endl;
}

Engine::~Engine() {
    std::cout << "Trading Engine shutting down" << std::endl;
    
    // Clear all orders from the order books
    {
        std::lock_guard<std::mutex> buyLock(buyOrdersMutex);
        for (auto& [price, queue] : buyOrders) {
            while (!queue.empty()) {
                queue.pop();
            }
        }
        buyOrders.clear();
    }
    
    {
        std::lock_guard<std::mutex> sellLock(sellOrdersMutex);
        for (auto& [price, queue] : sellOrders) {
            while (!queue.empty()) {
                queue.pop();
            }
        }
        sellOrders.clear();
    }
    
    // Clear the lookup map
    {
        std::lock_guard<std::mutex> mapLock(orderMapMutex);
        orders.clear();
    }
} 