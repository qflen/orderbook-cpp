#include "Orderbook.h"
#include "Order.h"

#include <iostream>
#include <memory>

int main() {
    Orderbook ob;

    // SELL order at price 100.0 for quantity 5
    auto sellOrder = std::make_shared<Order>(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 5);
    ob.AddOrder(sellOrder);

    // BUY order at price 100.0 for quantity 5 (matches exactly)
    auto buyOrder = std::make_shared<Order>(2, Side::Buy, OrderType::GoodTillCancel, 100.0, 3);
    Trades trades = ob.AddOrder(buyOrder);

    // Formatted result
    for (const auto& trade : trades) {
        std::cout << "TRADE EXECUTED:\n"
                  << "  BuyOrderID: " << trade.taker.orderId
                  << " @ Price: " << trade.taker.price
                  << " for Qty: " << trade.taker.quantity << "\n"
                  << "  SellOrderID: " << trade.maker.orderId
                  << " @ Price: " << trade.maker.price
                  << " for Qty: " << trade.maker.quantity << "\n";
    }

    return 0;
}
