# Trading Engine

A high-performance trading engine implementation in C++ that supports order matching, cancellation, and thread-safe operations.

## Features

- Price-time priority order matching
  - First Priority: Price
  - Second Priority: Time (max fairness)
- Support for partial fills
- Thread-safe operations
- Client callback notifications
- Efficient order cancellation

## Assumptions

1. Orders are created and managed by the engine
   - Clients don't have OrderID when creating new orders
   - OrderID is received by client as a response
   - Makes Client implementation simpler

2. Optional Request Reject message
   - Implemented as a response to create/cancel order messages
   - Includes error status and reason

3. Order Type (Buy or Sell)
   - Added to the original protocol
   - Required for proper order matching

## Architecture

The engine consists of three main components:

- **Engine**: Singleton class managing the order book and matching logic
- **Client**: Interface for client implementations
- **Order**: Data structure representing orders

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./trading_engine
```

## Testing

The engine includes a test suite that verifies:
- Order matching
- Partial fills
- Cancellation
- Thread safety
- Client notifications

## Performance

The engine is optimized for:
- Low latency order processing
- Efficient memory usage
- Thread-safe operations
- Minimal lock contention
