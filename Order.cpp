#include "Order.h"

Order::Order(OrderId id, Side side, OrderType type, Price price, Quantity qty)
    : id_(id), side_(side), type_(type), price_(price), initialQuantity_(qty) {}

OrderId Order::GetOrderId() const { return id_; }
Side Order::GetSide() const { return side_; }
OrderType Order::GetOrderType() const { return type_; }
Price Order::GetPrice() const { return price_; }
Quantity Order::GetInitialQuantity() const { return initialQuantity_; }
Quantity Order::GetRemainingQuantity() const { return initialQuantity_ - filledQuantity_; }
bool Order::IsFilled() const { return filledQuantity_ >= initialQuantity_; }

void Order::Fill(Quantity quantity) {
    if (filledQuantity_ + quantity > initialQuantity_)
        quantity = initialQuantity_ - filledQuantity_;
    filledQuantity_ += quantity;
}

void Order::ToGoodTillCancel(Price worstPrice) {
    type_ = OrderType::GoodTillCancel;
    price_ = worstPrice;
}
