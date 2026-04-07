// order_book.cpp                                                                                              -*-C++-*-

#include "order_book.h"

#include <cassert>
#include <iostream>
#include <iterator>

namespace order_book {

cr::seconds OrderBook::s_marketOpenTime = cr::hours(9) + cr::minutes(30);
cr::seconds OrderBook::s_marketCloseTime = cr::hours(16);

bool OrderBook::isMarketInOpenHours(const cr::time_point<cr::system_clock>& timePoint) {
    const auto [_, localDay, localSecond, secondsWithinDay] = getLocalTimeInfo(timePoint);

    if (!isWeekday(localDay)) {
        return false;
    }

    return s_marketOpenTime <= secondsWithinDay && secondsWithinDay < s_marketCloseTime;
}

OrderBook::LocalTimeInfo OrderBook::getLocalTimeInfo(const cr::time_point<cr::system_clock>& timePoint) {
    const cr::time_zone* timeZone = cr::current_zone();
    const cr::zoned_time zonedTime = cr::zoned_time(timeZone, timePoint);

    const cr::local_time<cr::system_clock::duration> localTime = zonedTime.get_local_time();
    const cr::local_days localDay = cr::floor<cr::days>(localTime);
    const cr::local_seconds localSecond = cr::floor<cr::seconds>(localTime);
    const cr::seconds secondWithinDay = localSecond - localDay;

    return {
        .localTime = localTime,
        .localDay = localDay,
        .localSecond = localSecond,
        .secondsWithinDay = secondWithinDay
    };
}

bool OrderBook::isWeekday(const cr::local_days& localDay) {
    const cr::weekday dayInWeek = cr::weekday(localDay);
    return !(dayInWeek == cr::Saturday || dayInWeek == cr::Sunday);
}

cr::time_point<cr::system_clock> OrderBook::calculateNextOpenTime(const cr::time_point<cr::system_clock>& timePoint) {
    return calculateNextEventTime(true, timePoint);
}

cr::time_point<cr::system_clock> OrderBook::calculateNextCloseTime(const cr::time_point<cr::system_clock>& timePoint) {
    return calculateNextEventTime(false, timePoint);
}

cr::time_point<cr::system_clock> OrderBook::calculateNextEventTime(
    const bool isNextEventOpen, const cr::time_point<cr::system_clock>& timePoint) {
    const auto [_, localDay, localSecond, secondsWithinDay] = getLocalTimeInfo(timePoint);
    // `localSecond` is not used in this function,
    // but `[_, localDay, _, secondsWithinday]` will raise warning of multiple unnamed placeholders until C++26.
    // So casting it to `void` to suppress the "unused variable" warning.
    // Another option is adding `[[maybe_unused]]` before the entire declaration.
    (void)localSecond;

    const cr::seconds eventTimeWithinDay = isNextEventOpen ? s_marketOpenTime : s_marketCloseTime;
    const cr::local_days eventDay =
        isWeekday(localDay) && secondsWithinDay < eventTimeWithinDay ? localDay : calculateNextWeekday(localDay);
    const cr::local_seconds eventSecond = eventDay + eventTimeWithinDay;
    const cr::zoned_time zonedEvent = cr::zoned_time(cr::current_zone(), eventSecond);
    return zonedEvent.get_sys_time();
}

cr::local_days OrderBook::calculateNextWeekday(const cr::local_days& localDay) {
    const cr::weekday dayInWeek = cr::weekday(localDay);
    if (dayInWeek == cr::Friday) {
        return localDay + cr::days(3);
    }
    if (dayInWeek == cr::Saturday) {
        return localDay + cr::days(2);
    }
    return localDay + cr::days(1);
}

void OrderBook::openCloseMarket() {
    while (true) {
        const MarketTimeStatus marketTimeStatus = d_marketTimeStatus.load(std::memory_order_acquire);
        const cr::time_point nextWakeupTimePoint = marketTimeStatus.isMarketOpen
                                                   ? calculateNextCloseTime(marketTimeStatus.timePoint)
                                                   : calculateNextOpenTime(marketTimeStatus.timePoint);
        bool wakeupDueToShutdown = false;
        {
            std::unique_lock marketLock(d_marketMutex);
            wakeupDueToShutdown =  d_marketConditionVariable.wait_until(
                marketLock,nextWakeupTimePoint,[this] { return d_isShuttingDown; });
        }

        const cr::time_point nowTimePoint = cr::system_clock::now();
        
        // If wakeup because of the entire system is shutting down, close market and return
        if (wakeupDueToShutdown) {
            onMarketClose(nowTimePoint);
            return;
        }
        
        if ( isMarketInOpenHours(nowTimePoint) ) {
            onMarketOpen(nowTimePoint);
        } else {
            onMarketClose(nowTimePoint);
        }
    }
}

void OrderBook::onMarketOpen(const cr::time_point<cr::system_clock>& timePoint) {
    std::scoped_lock ordersLock(d_ordersMutex);
    d_marketTimeStatus.store(MarketTimeStatus(timePoint, true), std::memory_order_release);
    addPendingLimitOrders();
}

void OrderBook::addPendingLimitOrders() {
    for (auto it = d_pendingLimitOrders.cbegin(); it != d_pendingLimitOrders.cend(); ) {
        Order order = it->second;
        it = d_pendingLimitOrders.erase(it);
        addLimitOrder(order);
    }
}

void OrderBook::onMarketClose(const cr::time_point<cr::system_clock>& timePoint) {
    std::scoped_lock ordersLock(d_ordersMutex);
    d_marketTimeStatus.store(MarketTimeStatus(timePoint, false), std::memory_order_release);
    cancelRemainingDayOrders();
}

void OrderBook::cancelRemainingDayOrders() {
    std::vector<OrderId> dayOrderIds; 
    for (const auto& [_, orders] : d_bids) {
        for (const Order& order : orders) {
            if (order.getOrderType() != OrderType::Day) {
                continue;
            }
            dayOrderIds.push_back(order.getId());
        }
    }
    for (const auto& [_, orders] : d_asks) {
        for (const Order& order : orders) {
            if (order.getOrderType() != OrderType::Day) {
                continue;
            }
            dayOrderIds.push_back(order.getId());
        }
    }
    for (const OrderId orderId : dayOrderIds) {
        cancelExistingOrder(orderId);
    }
}

std::vector<Trade> OrderBook::addLimitOrder(Order& order) {
    bool shouldAdd = shouldAddLimitOrder(order);
    return shouldAdd ? addOrder(order) : std::vector<Trade>();
}

bool OrderBook::shouldAddLimitOrder(const Order& order) const {
    if (order.getOrderType() == OrderType::ImmediateOrCancel &&
            !canMatchLimitOrder(order.getSide(), order.getPrice())) {
        std::cout << "No satisfying order existed for this Immediate-or-Cancel order"
                  << ". orderId=" << order.getId()
                  << ", side=" << order.getSide()
                  << ", price=" << order.getPrice()
                  << ".\n";
        return false;
    }
    if (order.getOrderType() == OrderType::FillOrKill && !canFullyFillLimitOrder(order)) {
        std::cout << "Cannot fill this Fill-or-Kill order"
                  << ". orderId=" << order.getId()
                  << ", side=" << order.getSide()
                  << ", price=" << order.getPrice()
                  << ", remainingQuantity=" << order.getRemainingQuantity()
                  << ".\n";
        return false;
    }
    return true;
}

bool OrderBook::canMatchLimitOrder(const Side orderSide, const Price orderPrice) const {
    if (orderSide == Side::Buy) {
        return d_asks.empty() ? false : orderPrice >= d_asks.begin()->first;
    }
    if (orderSide == Side::Sell) {
        return d_bids.empty() ? false : orderPrice <= d_bids.begin()->first;
    }
    assert(false);
    return false;
}

bool OrderBook::canFullyFillLimitOrder(const Order& order) const {
    Quantity quantityToFill = order.getRemainingQuantity();

    if (order.getSide() == Side::Buy) {
        for (const auto [askPrice, volume] : d_askVolumes) {
            if (askPrice > order.getPrice()) {
                return false;
            }
            if (volume >= quantityToFill) {
                return true;
            }
            quantityToFill -= volume;
        }
        return false;
    }
    if (order.getSide() == Side::Sell) {
        for (const auto& [bidPrice, volume] : d_bidVolumes) {
            if (bidPrice < order.getPrice()) {
                return false;
            }
            if (volume >= quantityToFill) {
                return true;
            }
            quantityToFill -= volume;
        }
        return false;
    }
    assert(false);
    return false;
}

bool OrderBook::canMatchMarketOrder(const Side orderSide) const {
    return orderSide == Side::Buy ? !d_asks.empty() : !d_bids.empty();
}

Price OrderBook::decideMarketOrderPrice(const Side orderSide) {
    return orderSide == Side::Buy ? std::prev(d_asks.end())->first : std::prev(d_bids.end())->first;
}

std::vector<Trade> OrderBook::addOrder(Order& order) {
    OrderId orderId = order.getId();
    OrderType orderType = order.getOrderType();
    Side side = order.getSide();
    Price price = order.getPrice();

    if (side == Side::Buy) {
        d_bids[price].push_back(order);
        d_orderMap.emplace(orderId,
            OrderLocation{.side = side, .price = price, .listIter = std::prev(d_bids[price].end())});
        d_bidVolumes[price] += order.getRemainingQuantity();
    } else {
        d_asks[price].push_back(order);
        d_orderMap.emplace(orderId,
            OrderLocation{.side = side, .price = price, .listIter = std::prev(d_asks[price].end())});
        d_askVolumes[price] += order.getRemainingQuantity();
    }

    std::vector trades = matchExistingOrders();

    cancelRemainingQuantityAfterMatching(orderId, orderType);

    return trades;
}

std::vector<Trade> OrderBook::matchExistingOrders() {
    std::vector<Trade> trades;
    trades.reserve(d_orderMap.size());

    while (!d_bids.empty() && !d_asks.empty()) {
        const Price bestBidPrice = d_bids.cbegin()->first;
        const Price bestAskPrice = d_asks.cbegin()->first;

        if (bestBidPrice < bestAskPrice) {
            break;
        }

        std::list<Order>& bestBids = d_bids.begin()->second;
        std::list<Order>& bestAsks = d_asks.begin()->second;

        while (!bestBids.empty() && !bestAsks.empty()) {
            Order& earliestBid = bestBids.front();
            Order& earliestAsk = bestAsks.front();

            const Quantity quantityToFill = std::min(
                earliestBid.getRemainingQuantity(), earliestAsk.getRemainingQuantity());
            earliestBid.Fill(quantityToFill);
            d_bidVolumes[bestBidPrice] -= quantityToFill;
            earliestAsk.Fill(quantityToFill);
            d_askVolumes[bestAskPrice] -= quantityToFill;

            const OrderId bidOrderId = earliestBid.getId();
            const OrderId askOrderId = earliestAsk.getId();

            // Trade price is the price of the earlier order (the "sitting" order) which has a lower order ID.
            const Price tradePrice = bidOrderId < askOrderId ? bestBidPrice : bestAskPrice;

            if (earliestBid.isFilled()) {
                bestBids.pop_front();
                d_orderMap.erase(bidOrderId);
            }
            if (earliestAsk.isFilled()) {
                bestAsks.pop_front();
                d_orderMap.erase(askOrderId);
            }
            
            Trade::Args tradeArgs = {
                .bidOrderId = bidOrderId,
                .askOrderId = askOrderId,
                .price = tradePrice,
                .quantity = quantityToFill,
            };
            trades.emplace_back(tradeArgs);
        }

        eraseBestPriceLevelWhenEmpty();
    }

    return trades;
}

void OrderBook::eraseBestPriceLevelWhenEmpty() {
    const auto& [bestBidPrice, bestBids] = *d_bids.cbegin();
    const auto& [bestAskPrice, bestAsks] = *d_asks.cbegin();

    if (bestBids.empty()) {
        d_bids.erase(bestBidPrice);

        assert(d_bidVolumes.at(bestBidPrice) == Quantity{0});
        d_bidVolumes.erase(bestBidPrice);
    }
    if (bestAsks.empty()) {
        d_asks.erase(bestAskPrice);

        assert(d_askVolumes.at(bestAskPrice) == Quantity{0});
        d_askVolumes.erase(bestAskPrice);
    }
}

void OrderBook::cancelRemainingQuantityAfterMatching(const OrderId orderId, const OrderType orderType) {
    bool shouldCancelRemaining = orderType == OrderType::ImmediateOrCancel || orderType == OrderType::Market;

    if (shouldCancelRemaining && d_orderMap.contains(orderId)) {
        cancelExistingOrder(orderId);
    }
}

void OrderBook::cancelExistingOrder(const OrderId orderId) {
    const auto [side, price, listIter] = d_orderMap[orderId];
    const Quantity quantity = listIter->getRemainingQuantity();

    d_orderMap.erase(orderId);

    if (side == Side::Buy) {
        d_bids[price].erase(listIter);
        d_bidVolumes[price] -= quantity;

        if (d_bids[price].empty()) {
            d_bids.erase(price);

            assert(d_bidVolumes.at(price) == Quantity{0});
            d_bidVolumes.erase(price);
        }
    } else {
        d_asks[price].erase(listIter);
        d_askVolumes[price] -= quantity;

        if (d_asks[price].empty()) {
            d_asks.erase(price);

            assert(d_askVolumes.at(price) == Quantity{0});
            d_askVolumes.erase(price);
        }
    }
}

std::vector<Trade> OrderBook::createAddLimitOrder(
    const OrderType orderType, const Side side, const Price price, const Quantity initialQuantity) {
    std::scoped_lock ordersLock(d_ordersMutex);
    
    const OrderId orderId = d_nextOrderId++;
    Order order(orderId, orderType, side, price, initialQuantity);

    if (!d_marketTimeStatus.load(std::memory_order_acquire).isMarketOpen) {
        std::cout << "Market is closed. Limit order has been created. "
                  << "orderId=" << orderId
                  << "It will be added to order book when market opens.\n";
        d_pendingLimitOrders.emplace(orderId, order);
        return {};
    }

    return addLimitOrder(order);
}

std::vector<Trade> OrderBook::createAddMarketOrder(const Side side, const Quantity quantity) {
    std::scoped_lock ordersLock(d_ordersMutex);

    if (!d_marketTimeStatus.load(std::memory_order_acquire).isMarketOpen) {
        std::cout << "Market is closed. Cannot add Market Order.\n";
        return {};
    }

    const OrderId orderId = d_nextOrderId++;
    Order order(orderId, OrderType::Market, side, std::nullopt, quantity);

    if (!canMatchMarketOrder(side)) {
        return {};
    }

    order.setPrice(decideMarketOrderPrice(side));
    return addOrder(order);
}

void OrderBook::cancelOrder(const OrderId orderId) {
    std::scoped_lock ordersLock(d_ordersMutex);

    if (!d_orderMap.contains(orderId)) {
        std::cerr << "Cannot find order. Cancellation failed. orderId=" << orderId << ".\n";
        return;
    }

    cancelExistingOrder(orderId);
}

} // namespace order_book