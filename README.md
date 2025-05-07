# orderbook-cpp

Multi‐type C++17 limit order book (GTC, GFD, Market, FOK, FAK) with Catch2 unit tests.

## Build

```bash
g++ -O3 -std=c++17 Order.cpp Orderbook.cpp tests.cpp -o tests
```

## Usage

```cpp
#include "Orderbook.h"
#include "Order.h"

// enable prune thread (default)
Orderbook ob;

// disable prune thread (for fast tests)
// Good-For-Day orders will not be auto-cancelled
Orderbook ob(false);
```

## Testing

Run the Catch2 test suite:

```bash
./tests
```