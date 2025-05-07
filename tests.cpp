#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "Orderbook.h"
#include "Order.h"
#include <memory>

auto mk(OrderId id, Side side, OrderType type, Price price, Quantity qty) {
    return std::make_shared<Order>(id, side, type, price, qty);
}

TEST_CASE("Limit order fully matches", "[limit-full]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 5));
    auto trades = ob.AddOrder(mk(2, Side::Buy, OrderType::GoodTillCancel, 100.0, 5));
    REQUIRE(trades.size() == 1);
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("Limit order partially matches", "[limit-partial]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 10));
    auto trades = ob.AddOrder(mk(2, Side::Buy, OrderType::GoodTillCancel, 100.0, 6));
    REQUIRE(trades.size() == 1);
    REQUIRE(ob.Size() == 1);
    REQUIRE(ob.GetOrderInfos().asks[0].totalQuantity == 4);
}

TEST_CASE("Multi-level depth matching", "[depth-match]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 3));
    ob.AddOrder(mk(2, Side::Sell, OrderType::GoodTillCancel, 101.0, 4));
    auto trades = ob.AddOrder(mk(3, Side::Buy, OrderType::GoodTillCancel, 101.0, 7));
    REQUIRE(trades.size() == 2);
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("FillOrKill fails when insufficient", "[fok-fail]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 2));
    auto trades = ob.AddOrder(mk(2, Side::Buy, OrderType::FillOrKill, 100.0, 5));
    REQUIRE(trades.empty());
    REQUIRE(ob.Size() == 1);
}

TEST_CASE("FillOrKill succeeds when fully matched", "[fok-pass]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 3));
    ob.AddOrder(mk(2, Side::Sell, OrderType::GoodTillCancel, 100.0, 2));
    auto trades = ob.AddOrder(mk(3, Side::Buy, OrderType::FillOrKill, 100.0, 5));
    REQUIRE(trades.size() == 2);
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("FillAndKill matches whatever is available", "[fak]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 3));
    auto trades = ob.AddOrder(mk(2, Side::Buy, OrderType::FillAndKill, 100.0, 10));
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].taker.quantity == 3);
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("Market buy with liquidity", "[market-buy]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 100.0, 4));
    auto trades = ob.AddOrder(mk(2, Side::Buy, OrderType::Market, 0.0, 4));
    REQUIRE(trades.size() == 1);
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("Market sell with no bids", "[market-sell-empty]") {
    Orderbook ob(false);
    auto trades = ob.AddOrder(mk(1, Side::Sell, OrderType::Market, 0.0, 5));
    REQUIRE(trades.empty());
    REQUIRE(ob.Size() == 0);
}

TEST_CASE("Cancel order removes it", "[cancel]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodTillCancel, 101.0, 7));
    REQUIRE(ob.Size() == 1);
    ob.CancelOrder(1);
    REQUIRE(ob.Size() == 0);
}


TEST_CASE("GoodForDay manual prune simulation", "[gfd]") {
    Orderbook ob(false);
    ob.AddOrder(mk(1, Side::Sell, OrderType::GoodForDay, 100.0, 5));
    ob.CancelOrder(1);  // simulate manual pruning
    REQUIRE(ob.Size() == 0);
}
