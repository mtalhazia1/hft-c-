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
