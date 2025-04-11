#include "Engine.h"
#include "Client.h"
#include "Types.h"
#include <thread>
#include <chrono>
#include <random>
#include <memory>
#include <atomic>
#include <iostream>
#include <iomanip>

std::atomic<int> totalOrdersProcessed(0);
std::atomic<int> totalTradesExecuted(0);
std::atomic<int> totalOrdersCanceled(0);

/*
README.md
# C++ Matching Engine

A simple yet functional matching engine for a single asset, implementing a continuous limit order book model.

## Features

- Price-time priority order matching (First Priorty : Price, if Price Match 2nd Priority Time max Fairness)
- Support for partial fills
- Thread-safe operations
- Client callback notifications
- Efficient order cancellation

##Assumptions:
    1. Orders are created and managed by the engine hence Clinet donot have OrderID when        creating new orders instead the order ID is recived by cliet as a response
    Making Client Simpler
    2. Optional Request Reject message is implimented as a response to create/cancel order messages. With error status and reason.
    3. Order Type (Buy or Sell) was missing in the orignal protocol, which was added.
    
## Architecture

The engine consists of three main components:

1. **Engine** - Singleton class managing the order book and matching logic
2. **Order** - Structure representing buy/sell orders
3. **Client** - Interface for client notifications

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./matching_engine
```
*/

void printTestSummary(int expectedOrders) {
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total orders processed: " << totalOrdersProcessed << "/" << expectedOrders << std::endl;
    std::cout << "Total trades executed: " << totalTradesExecuted << std::endl;
    std::cout << "Total orders canceled: " << totalOrdersCanceled << std::endl;
}

void clientThread(std::shared_ptr<Client> client, Engine& engine, int numOrders) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> priceDist(90, 110);
    std::uniform_int_distribution<> amountDist(1, 100);
    std::uniform_int_distribution<> orderTypeDist(0, 1); // 0 for buy, 1 for sell

    for (int i = 0; i < numOrders; ++i) {
        Price price(priceDist(gen));
        Amount amount(amountDist(gen));
        OrderType type = orderTypeDist(gen) == 0 ? OrderType::BUY : OrderType::SELL;
        
        // Place order
        auto response = engine.placeOrder(type, price, amount, client);
        
        if (response.status == ResponseStatus::SUCCESS) {
            totalOrdersProcessed++;
            std::cout << "[Progress: " << totalOrdersProcessed << "/" << (numOrders * 2) 
                      << " orders] " << client->getName() << " placed " 
                      << (type == OrderType::BUY ? "BUY" : "SELL")
                      << " order " << (i + 1) << "/" << numOrders << std::endl;
            
            // Randomly cancel some orders (every third order)
            if (i % 3 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto cancelResponse = engine.cancelOrder(response.orderId, client);
                if (cancelResponse.status == ResponseStatus::SUCCESS) {
                    totalOrdersCanceled++;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main() {
    const int ordersPerClient = 10;
    std::cout << "Starting trading engine test with " << ordersPerClient << " orders per client..." << std::endl;
    Engine engine;
    
    // Create two clients with smart pointers
    auto client1 = std::make_shared<Client>("Client1");
    auto client2 = std::make_shared<Client>("Client2");

    auto startTime = std::chrono::high_resolution_clock::now();

    // Create threads for each client
    std::thread client1Thread(clientThread, client1, std::ref(engine), ordersPerClient);
    std::thread client2Thread(clientThread, client2, std::ref(engine), ordersPerClient);

    // Wait for threads to complete
    client1Thread.join();
    client2Thread.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "\n=== Test Completed ===" << std::endl;
    std::cout << "Duration: " << duration.count() << "ms" << std::endl;
    printTestSummary(ordersPerClient * 2);

    return 0;
} 