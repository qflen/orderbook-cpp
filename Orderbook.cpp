#include "Orderbook.h"
#include <numeric>
#include <ctime>

Orderbook::Orderbook(bool startThread)
    : usePruneThread_(startThread)
{
    if (usePruneThread_)
        pruneThread_ = std::thread(&Orderbook::pruneThreadFunc, this);
}

Orderbook::~Orderbook() {
    if (usePruneThread_) {
        {
            std::lock_guard lk(mtx_);
            shutdown_ = true;
        }
        cv_.notify_one();
        pruneThread_.join();
    }
}

Trades Orderbook::AddOrder(OrderPointer order) {
    std::lock_guard lk(mtx_);
    if (orders_.find(order->GetOrderId()) != orders_.end())
        return {};

    // Market orders: match immediately against the opposite side
    if (order->GetOrderType() == OrderType::Market) {
        Trades trades;
        Quantity remaining = order->GetInitialQuantity();

        if (order->GetSide() == Side::Buy) {
            while (remaining > 0 && !asks_.empty()) {
                auto &level = asks_.begin()->second;
                auto maker = level.front();
                Quantity avail = maker->GetRemainingQuantity();
                Quantity qty = std::min(avail, remaining);

                maker->Fill(qty);
                remaining -= qty;

                trades.push_back(Trade{
                    { order->GetOrderId(), order->GetPrice(), qty },
                    { maker->GetOrderId(), maker->GetPrice(), qty }
                });

                if (maker->IsFilled()) {
                    level.pop_front();
                    orders_.erase(maker->GetOrderId());
                }
                if (level.empty())
                    asks_.erase(asks_.begin());
            }
        } else { // Side::Sell
            while (remaining > 0 && !bids_.empty()) {
                auto &level = bids_.begin()->second;
                auto maker = level.front();
                Quantity avail = maker->GetRemainingQuantity();
                Quantity qty = std::min(avail, remaining);

                maker->Fill(qty);
                remaining -= qty;

                trades.push_back(Trade{
                    { order->GetOrderId(), order->GetPrice(), qty },
                    { maker->GetOrderId(), maker->GetPrice(), qty }
                });

                if (maker->IsFilled()) {
                    level.pop_front();
                    orders_.erase(maker->GetOrderId());
                }
                if (level.empty())
                    bids_.erase(bids_.begin());
            }
        }

        return trades;
    }

    // FillAndKill / FillOrKill checks
    if (order->GetOrderType() == OrderType::FillAndKill &&
        !canMatch(order->GetSide(), order->GetPrice()))
        return {};
    if (order->GetOrderType() == OrderType::FillOrKill &&
        !canFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
        return {};

    // Insert into book
    auto &dq = (order->GetSide() == Side::Buy
                ? bids_[order->GetPrice()]
                : asks_[order->GetPrice()]);
    dq.push_back(order);
    orders_[order->GetOrderId()] = { order, std::prev(dq.end()) };
    onOrderAdded(order);

    return matchCrossedOrders();
}


void Orderbook::CancelOrder(OrderId id) {
    std::lock_guard lk(mtx_);
    removeOrderInternal(id);
}

Trades Orderbook::ModifyOrder(OrderPointer modified, OrderType) {
    {
        std::lock_guard lk(mtx_);
        if (orders_.find(modified->GetOrderId()) == orders_.end())
            return {};
        removeOrderInternal(modified->GetOrderId());
    }
    modified->ToGoodTillCancel(modified->GetPrice());
    return AddOrder(modified);
}

std::size_t Orderbook::Size() const {
    std::lock_guard lk(mtx_);
    return orders_.size();
}

OrderBookLevelInfos Orderbook::GetOrderInfos() const {
    std::lock_guard lk(mtx_);
    auto summarize = [](auto const& book) {
        std::vector<LevelInfo> out;
        for (auto const& [price, dq] : book) {
            Quantity total = std::accumulate(
                dq.begin(), dq.end(), Quantity(0),
                [](Quantity sum, OrderPointer const& o) {
                    return sum + o->GetRemainingQuantity();
                });
            out.push_back({ price, total, static_cast<int>(dq.size()) });
        }
        return out;
    };
    return { summarize(bids_), summarize(asks_) };
}

bool Orderbook::canMatch(Side side, Price price) const {
    if (side == Side::Buy)
        return !asks_.empty() && price >= asks_.begin()->first;
    else
        return !bids_.empty() && price <= bids_.begin()->first;
}

bool Orderbook::canFullyFill(Side side, Price price, Quantity quantity) const {
    if (side == Side::Buy) {
        for (auto const& [p, dq] : asks_) {
            if (p > price) break;
            for (auto const& o : dq) {
                Quantity avail = o->GetRemainingQuantity();
                if (avail >= quantity) return true;
                quantity -= avail;
                if (quantity == 0) return true;
            }
        }
    } else {
        for (auto const& [p, dq] : bids_) {
            if (p < price) break;
            for (auto const& o : dq) {
                Quantity avail = o->GetRemainingQuantity();
                if (avail >= quantity) return true;
                quantity -= avail;
                if (quantity == 0) return true;
            }
        }
    }
    return false;
}

void Orderbook::removeOrderInternal(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return;
    auto const& [order, dqIt] = it->second;
    auto& dq = (order->GetSide() == Side::Buy
                ? bids_[order->GetPrice()]
                : asks_[order->GetPrice()]);
    dq.erase(dqIt);
    if (dq.empty()) {
        if (order->GetSide() == Side::Buy) bids_.erase(order->GetPrice());
        else asks_.erase(order->GetPrice());
    }
    orders_.erase(it);
    updateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelAction::Remove);
    onOrderCancelled(order);
}

Trades Orderbook::matchCrossedOrders() {
    Trades trades;
    while (!bids_.empty() && !asks_.empty()) {
        auto& bidLevel = bids_.begin()->second;
        auto& askLevel = asks_.begin()->second;
        auto bid = bidLevel.front();
        auto ask = askLevel.front();
        if (bid->GetPrice() < ask->GetPrice()) break;
        Quantity qty = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
        bid->Fill(qty);
        ask->Fill(qty);
        trades.push_back(Trade{
            { bid->GetOrderId(), bid->GetPrice(), qty },
            { ask->GetOrderId(), ask->GetPrice(), qty }
        });
        onOrderMatched(bid->GetPrice(), qty, bid->IsFilled());
        onOrderMatched(ask->GetPrice(), qty, ask->IsFilled());
        if (bid->IsFilled()) {
            bidLevel.pop_front();
            orders_.erase(bid->GetOrderId());
        }
        if (ask->IsFilled()) {
            askLevel.pop_front();
            orders_.erase(ask->GetOrderId());
        }
        if (bidLevel.empty()) bids_.erase(bids_.begin());
        if (askLevel.empty()) asks_.erase(asks_.begin());
    }
    auto cleanupFAK = [this](auto& book) {
        if (book.empty()) return;
        auto& [price, dq] = *book.begin();
        if (!dq.empty() && dq.front()->GetOrderType() == OrderType::FillAndKill && !dq.front()->IsFilled()) {
            orders_.erase(dq.front()->GetOrderId());
            dq.pop_front();
            if (dq.empty()) book.erase(price);
        }
    };
    cleanupFAK(bids_);
    cleanupFAK(asks_);
    return trades;
}

void Orderbook::updateLevelData(Price price, Quantity qty, LevelAction action) {
    auto& lvl = levelData_[price];
    if (action == LevelAction::Add) {
        lvl.totalQuantity += qty;
        lvl.count++;
    } else if (action == LevelAction::Match) {
        lvl.totalQuantity -= qty;
    } else {
        lvl.totalQuantity -= qty;
        lvl.count--;
        if (lvl.count <= 0) levelData_.erase(price);
    }
}

void Orderbook::onOrderAdded(const OrderPointer&) {}
void Orderbook::onOrderCancelled(const OrderPointer&) {}
void Orderbook::onOrderMatched(Price, Quantity, bool) {}

std::chrono::system_clock::time_point Orderbook::computeNextPruneTime() const {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    tm.tm_hour = 16; tm.tm_min = 0; tm.tm_sec = 0;
    auto next = system_clock::from_time_t(std::mktime(&tm));
    if (next <= now) next += hours(24);
    return next;
}

void Orderbook::pruneThreadFunc() {
    std::unique_lock lk(mtx_);
    while (!shutdown_) {
        auto next = computeNextPruneTime();
        cv_.wait_until(lk, next, [this]{ return shutdown_.load(); });
        if (shutdown_) break;
        OrderIds toCancel;
        for (auto const& [id, entry] : orders_) {
            if (entry.first->GetOrderType() == OrderType::GoodForDay)
                toCancel.push_back(id);
        }
        lk.unlock();
        for (auto id : toCancel) CancelOrder(id);
        lk.lock();
    }
}
