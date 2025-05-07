# orderbook-cpp

Multi-type C++17 limit order book (GTC, GFD, Market, FOK, FAK) with Catch2 unit tests.

## Build

```bash
g++ -O3 -std=c++17 Order.cpp Orderbook.cpp tests.cpp -o tests
```

## Usage

```cpp
#include "Orderbook.h"
#include "Order.h"
#include <memory>
#include <iostream>

// disable prune thread for simplicity
Orderbook ob(false);

// add a limit sell @100×5
ob.AddOrder(std::make_shared<Order>(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 5));

// match with a limit buy @100×5
auto trades = ob.AddOrder(std::make_shared<Order>(2, Side::Buy, OrderType::GoodTillCancel, 100.0, 5));
for (auto& t : trades)
    std::cout << "Trade: " << t.taker.quantity << " @ " << t.taker.price << "\n";
```

## Testing

Run the Catch2 test suite:

```bash
./tests
```

## Thread Safety

All public methods (`AddOrder`, `CancelOrder`, `ModifyOrder`, `GetOrderInfos`, etc.) lock a single `std::mutex mtx_` via:

```cpp
std::lock_guard<std::mutex> lk(mtx_);
```

- **Mutual exclusion**: only one thread may access the book at a time.  
- **RAII safety**: the lock is released automatically when the guard goes out of scope.

By default (`Orderbook ob;`) a background prune thread is also started:

```cpp
pruneThread_ = std::thread(&Orderbook::pruneThreadFunc, this);
```

It uses the same mutex and a `std::condition_variable::wait_until` to wake at 16:00 each day and cancel all `GoodForDay` orders. To disable it (e.g., in tests), construct with:

```cpp
Orderbook ob(false);
```
