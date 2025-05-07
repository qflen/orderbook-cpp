#pragma once

#include <cstdint>

using OrderId = std::size_t;
using Price = double;
using Quantity = int;

enum class Side { Buy, Sell };
enum class OrderType { GoodTillCancel, GoodForDay, Market, FillOrKill, FillAndKill };

class Order {
public:
    Order(OrderId id, Side side, OrderType type, Price price, Quantity qty);

    OrderId GetOrderId() const;
    Side GetSide() const;
    OrderType GetOrderType() const;
    Price GetPrice() const;
    Quantity GetInitialQuantity() const;
    Quantity GetRemainingQuantity() const;
    bool IsFilled() const;

    void Fill(Quantity quantity);
    void ToGoodTillCancel(Price worstPrice);

private:
    OrderId id_;
    Side side_;
    OrderType type_;
    Price price_;
    Quantity initialQuantity_;
    Quantity filledQuantity_ = 0;
};
