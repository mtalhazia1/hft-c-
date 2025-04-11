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

// Remove extern declaration
// extern std::atomic<int> totalTradesExecuted;

// Remove singleton instance
// std::unique_ptr<Engine> Engine::instance = nullptr;
// std::mutex Engine::instanceMutex;

Engine::Engine() : nextOrderId(OrderId(0)), totalTradesExecuted(0) {
    std::cout << "Trading Engine started" << std::endl;
}

Response Engine::placeOrder(OrderType type, Price price, Amount amount, std::shared_ptr<Client> client) {
    if (!client) {
        return Response(ResponseStatus::INVALID_ORDER, "Invalid client");
    }

    if (amount.value <= 0 || price.value <= 0) {
        return Response(ResponseStatus::INVALID_ORDER, "Invalid amount or price");
    }

    // Create new order outside the lock
    OrderId orderId = nextOrderId.load();
    nextOrderId.store(OrderId(orderId.value + 1));
    auto order = std::make_shared<Order>(orderId, type, price, amount, client);

    auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);

    std::cout << "\n[Time: " << duration.count() << "μs] New order received: " 
              << (type == OrderType::BUY ? "BUY" : "SELL") 
              << " OrderId: " << orderId.value 
              << " Price: " << price.value 
              << " Amount: " << amount.value << std::endl;

    // Store order in lookup map before matching
    {
        std::lock_guard<std::mutex> lock(orderBookMutex);
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
    
    std::lock_guard<std::mutex> lock(orderBookMutex);
    
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
    
    std::cout << "\n[Time: " << duration.count() << "μs] Cancel request received for OrderId: " 
              << orderId.value << std::endl;
    
    // Find order in lookup map
    auto it = orders.find(orderId);
    if (it == orders.end()) {
        std::cout << "Order not found" << std::endl;
        return Response(ResponseStatus::ORDER_NOT_FOUND, "Order not found");
    }
    
    std::shared_ptr<Order> order = it->second;
    if (order->client != client) {
        std::cout << "Order does not belong to client" << std::endl;
        return Response(ResponseStatus::INVALID_ORDER, "Order does not belong to client");
    }
    
    // Remove from order book
    if (order->type == OrderType::BUY) {
        auto& queue = buyOrders[order->price];
        bool found = false;
        size_t size = queue.size();
        
        // Find and remove the order
        for (size_t i = 0; i < size; ++i) {
            auto front = std::move(queue.front());
            queue.pop();
            if (front->orderId != orderId) {
                queue.push(std::move(front));
            } else {
                found = true;
            }
        }
        
        // Remove price level if empty
        if (queue.empty()) {
            buyOrders.erase(order->price);
        }
    } else {
        auto& queue = sellOrders[order->price];
        bool found = false;
        size_t size = queue.size();
        
        // Find and remove the order
        for (size_t i = 0; i < size; ++i) {
            auto front = std::move(queue.front());
            queue.pop();
            if (front->orderId != orderId) {
                queue.push(std::move(front));
            } else {
                found = true;
            }
        }
        
        // Remove price level if empty
        if (queue.empty()) {
            sellOrders.erase(order->price);
        }
    }
    
    // Remove from lookup map
    orders.erase(orderId);
    
    std::cout << "Order cancelled. Current state:" << std::endl;
    logOrderBookState();
    
    return Response(ResponseStatus::SUCCESS, "Order cancelled successfully");
}

void Engine::matchOrders(std::shared_ptr<Order> newOrder) {
    if (!newOrder) return;

    std::cout << "\nAttempting to match order: " << newOrder->orderId.value << std::endl;

    // Create a vector to store trades that need to be executed
    std::vector<std::tuple<std::shared_ptr<Order>, std::shared_ptr<Order>, Price, Amount>> tradesToExecute;
    bool orderAddedToBook = false;

    {
        std::lock_guard<std::mutex> lock(orderBookMutex);

        if (newOrder->type == OrderType::BUY) {
            // Try to match with sell orders
            auto it = sellOrders.begin();
            while (it != sellOrders.end() && newOrder->remainingAmount.value > 0) {
                if (newOrder->price.value < it->first.value) {
                    std::cout << "No matching sell orders at acceptable price" << std::endl;
                    break;
                }

                auto& queue = it->second;
                while (!queue.empty() && newOrder->remainingAmount.value > 0) {
                    auto sellOrder = queue.front();
                    queue.pop();
                    
                    // Calculate trade amount
                    Amount tradeAmount(std::min(newOrder->remainingAmount.value, 
                                             sellOrder->remainingAmount.value));
                    Price tradePrice = sellOrder->price;
                    
                    // Store trade details for later execution
                    tradesToExecute.emplace_back(newOrder, sellOrder, tradePrice, tradeAmount);
                    
                    // Update remaining amounts
                    newOrder->remainingAmount.value -= tradeAmount.value;
                    sellOrder->remainingAmount.value -= tradeAmount.value;
                    
                    // If sell order still has remaining amount, push it back
                    if (sellOrder->remainingAmount.value > 0) {
                        queue.push(sellOrder);
                    }
                    
                    if (queue.empty()) {
                        it = sellOrders.erase(it);
                        break;
                    }
                }
                if (it != sellOrders.end()) ++it;
            }
        } else {
            // Try to match with buy orders
            auto it = buyOrders.begin();
            while (it != buyOrders.end() && newOrder->remainingAmount.value > 0) {
                if (newOrder->price.value > it->first.value) {
                    std::cout << "No matching buy orders at acceptable price" << std::endl;
                    break;
                }
                
                auto& queue = it->second;
                while (!queue.empty() && newOrder->remainingAmount.value > 0) {
                    auto buyOrder = queue.front();
                    queue.pop();
                    
                    // Calculate trade amount
                    Amount tradeAmount(std::min(newOrder->remainingAmount.value, 
                                             buyOrder->remainingAmount.value));
                    Price tradePrice = buyOrder->price;
                    
                    // Store trade details for later execution
                    tradesToExecute.emplace_back(buyOrder, newOrder, tradePrice, tradeAmount);
                    
                    // Update remaining amounts
                    newOrder->remainingAmount.value -= tradeAmount.value;
                    buyOrder->remainingAmount.value -= tradeAmount.value;
                    
                    // If buy order still has remaining amount, push it back
                    if (buyOrder->remainingAmount.value > 0) {
                        queue.push(buyOrder);
                    }
                    
                    if (queue.empty()) {
                        it = buyOrders.erase(it);
                        break;
                    }
                }
                if (it != buyOrders.end()) ++it;
            }
        }
        
        // If order wasn't fully matched, add remaining to book
        if (newOrder->remainingAmount.value > 0) {
            if (newOrder->type == OrderType::BUY) {
                buyOrders[newOrder->price].push(newOrder);
            } else {
                sellOrders[newOrder->price].push(newOrder);
            }
            orderAddedToBook = true;
        }

        // Log order book state while holding the lock
        if (orderAddedToBook) {
            logOrderBookState();
        }
    }

    // Execute trades and notify clients outside the lock
    for (const auto& [buyOrder, sellOrder, price, amount] : tradesToExecute) {
        std::cout << "Match found! Trade executed:" << std::endl;
        std::cout << "Buy OrderId: " << buyOrder->orderId.value 
                 << " Sell OrderId: " << sellOrder->orderId.value
                 << " Price: " << price.value
                 << " Amount: " << amount.value << std::endl;
        
        // Notify clients about the trade
        buyOrder->client->onOrderTraded(buyOrder->orderId, price, amount);
        sellOrder->client->onOrderTraded(sellOrder->orderId, price, amount);
        
        totalTradesExecuted++;
    }
}

void Engine::logOrderBookState() {
    std::cout << "\nOrder Book State:" << std::endl;
    
    std::cout << "Buy Orders (highest first):" << std::endl;
    for (const auto& [price, queue] : buyOrders) {
        std::cout << "Price " << price.value << ": " << queue.size() << " orders" << std::endl;
        // Show details of each order at this price level
        if (!queue.empty()) {
            const auto& order = queue.front();
            std::cout << "  - OrderId: " << order->orderId.value 
                      << " Amount: " << order->remainingAmount.value
                      << " Client: " << order->client->getName() << std::endl;
        }
    }
    
    std::cout << "Sell Orders (lowest first):" << std::endl;
    for (const auto& [price, queue] : sellOrders) {
        std::cout << "Price " << price.value << ": " << queue.size() << " orders" << std::endl;
        // Show details of each order at this price level
        if (!queue.empty()) {
            const auto& order = queue.front();
            std::cout << "  - OrderId: " << order->orderId.value 
                      << " Amount: " << order->remainingAmount.value
                      << " Client: " << order->client->getName() << std::endl;
        }
    }
    std::cout << std::endl;
}

Engine::~Engine() {
    std::lock_guard<std::mutex> lock(orderBookMutex);
    std::cout << "Trading Engine shutting down" << std::endl;
    
    // Clear all orders from the order books
    for (auto& [price, queue] : buyOrders) {
        while (!queue.empty()) {
            queue.pop();
        }
    }
    buyOrders.clear();
    
    for (auto& [price, queue] : sellOrders) {
        while (!queue.empty()) {
            queue.pop();
        }
    }
    sellOrders.clear();
    
    // Clear the lookup map
    orders.clear();
} 