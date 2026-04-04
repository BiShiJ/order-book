#include "order_book.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>

#include "enums.h"

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
    const auto [_, localDay, localSecond, secondsWithinDay] = getLocalTimeInfo(timePoint);

    const cr::local_days openDay =
        isWeekday(localDay) && secondsWithinDay < s_marketOpenTime ? localDay : calculateNextWeekday(localDay);
    const cr::local_seconds openSeconds = openDay + s_marketOpenTime;
    const cr::zoned_time zonedOpen = cr::zoned_time(cr::current_zone(), openSeconds);
    return zonedOpen.get_sys_time();
}

cr::time_point<cr::system_clock> OrderBook::calculateNextCloseTime(const cr::time_point<cr::system_clock>& timePoint) {
    const auto [_, localDay, localSecond, secondsWithinDay] = getLocalTimeInfo(timePoint);

    const cr::local_days closeDay =
        isWeekday(localDay) && secondsWithinDay < s_marketCloseTime ? localDay : calculateNextWeekday(localDay);
    const cr::local_seconds closeSeconds = closeDay + s_marketCloseTime;
    const cr::zoned_time zonedClose = cr::zoned_time(cr::current_zone(), closeSeconds);
    return zonedClose.get_sys_time();
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
    bool shouldAdd = shouldAddLimitOrder(
        order.getId(), order.getOrderType(), order.getSide(), order.getPrice());
    return shouldAdd ? addOrder(order) : std::vector<Trade>();
}

bool OrderBook::shouldAddLimitOrder(
        const OrderId orderId, const OrderType orderType, const Side side, const Price price) const {
    if (orderType == OrderType::ImmediateOrCancel && !canMatchLimitOrder(side, price)) {
        std::cout << "No satisfying order existed for this Immediate-or-Cancel order"
                  << ". orderId=" << orderId
                  << ", side=" << side
                  << ", price=" << price
                  << "\n";
        return false;
    }
    return true;
}

bool OrderBook::canMatchLimitOrder(const Side side, const Price price) const {
    if (side == Side::Buy) {
        return d_asks.empty() ? false : price >= d_asks.begin()->first;
    }
    return d_bids.empty() ? false : price <= d_bids.begin()->first;
}

bool OrderBook::canMatchMarketOrder(const Side side) const {
    return side == Side::Buy ? !d_asks.empty() : !d_bids.empty();
}

Price OrderBook::decideMarketOrderPrice(const Side side) {
    return side == Side::Buy ? std::prev(d_asks.end())->first : std::prev(d_bids.end())->first;
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
    } else {
        d_asks[price].push_back(order);
        d_orderMap.emplace(orderId,
            OrderLocation{.side = side, .price = price, .listIter = std::prev(d_asks[price].end())});
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
            earliestAsk.Fill(quantityToFill); 

            const OrderId bidOrderId = earliestBid.getId();
            const OrderId askOrderId = earliestAsk.getId();

            // Trade price is the price of the earlier order (the "sitting" order) which has a lower order ID.
            const Price tradePrice = bidOrderId < askOrderId ? earliestBid.getPrice() : earliestAsk.getPrice();

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

        if (bestBids.empty()) {
            d_bids.erase(bestBidPrice);
        }
        if (bestAsks.empty()) {
            d_asks.erase(bestAskPrice);
        }
    }

    return trades;
}

void OrderBook::cancelRemainingQuantityAfterMatching(const OrderId orderId, const OrderType orderType) {
    bool shouldCancelRemaining = orderType == OrderType::ImmediateOrCancel || orderType == OrderType::Market;

    if (shouldCancelRemaining && d_orderMap.contains(orderId)) {
        cancelExistingOrder(orderId);
    }
}

void OrderBook::cancelExistingOrder(const OrderId orderId) {
    const auto [side, price, listIter] = d_orderMap[orderId];
    d_orderMap.erase(orderId);

    if (side == Side::Buy) {
        d_bids[price].erase(listIter);
        if (d_bids[price].empty()) {
            d_bids.erase(price);
        }
    } else {
        d_asks[price].erase(listIter);
        if (d_asks[price].empty()) {
            d_asks.erase(price);
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
                  << "It will be added to order book when market opens";
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
        std::cerr << "Cannot find order. Cancellation failed. orderId=" << orderId << "\n";
        return;
    }

    cancelExistingOrder(orderId);
}

} // namespace order_book