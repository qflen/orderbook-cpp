#pragma once

#include "Order.h"
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <memory>

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::deque<OrderPointer>;
using OrderIds = std::vector<OrderId>;

struct TradeInfo {
    OrderId orderId;
    Price price;
    Quantity quantity;
};

struct Trade {
    TradeInfo taker;
    TradeInfo maker;
};

using Trades = std::vector<Trade>;

struct LevelInfo {
    Price price;
    Quantity totalQuantity;
    int count;
};

struct OrderBookLevelInfos {
    std::vector<LevelInfo> bids;
    std::vector<LevelInfo> asks;
};

class Orderbook {
public:
    Orderbook(bool startThread = true);
    ~Orderbook();

    std::size_t Size() const;
    OrderBookLevelInfos GetOrderInfos() const;

    void CancelOrder(OrderId);
    Trades AddOrder(OrderPointer);
    Trades ModifyOrder(OrderPointer, OrderType);

private:
    enum class LevelAction { Add, Remove, Match };

    using OrderEntry = std::pair<OrderPointer, OrderPointers::iterator>;

    std::map<Price, OrderPointers, std::greater<>> bids_;
    std::map<Price, OrderPointers> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    std::map<Price, LevelInfo> levelData_;

    mutable std::mutex mtx_;
    std::thread pruneThread_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_ = false;
    bool usePruneThread_ = true;

    void onOrderAdded(const OrderPointer&);
    void onOrderCancelled(const OrderPointer&);
    void onOrderMatched(Price, Quantity, bool);

    void updateLevelData(Price, Quantity, LevelAction);
    void removeOrderInternal(OrderId);

    bool canMatch(Side, Price) const;
    bool canFullyFill(Side, Price, Quantity) const;
    Trades matchCrossedOrders();

    void pruneThreadFunc();
    std::chrono::system_clock::time_point computeNextPruneTime() const;
};
