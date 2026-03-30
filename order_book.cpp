#include "order_book.h"

#include <iostream>

#include "enums.h"
#include "order.h"

namespace order_book {

void OrderBook::autoCancelDayOrders() {
    while(true) {
        if (d_isMarketOpen.load(std::memory_order_acquire)) {
            continue;
        }

        {
            std::scoped_lock<std::mutex> ordersLock(d_ordersMutex);

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

            return;
        }
    }
}

bool OrderBook::shouldAddLimitOrder(
        const OrderId orderId, const OrderType orderType, const Side side, const Price price) {
    if (orderType == OrderType::ImmediateOrCancel && !canMatchLimitOrder(side, price)) {
        std::cout << "No satisfying order existed for this Immediate-or-Cancel order"
                  << ". orderId=" << orderId
                  << ", side=" << side
                  << ", price=" << price
                  << std::endl;
        return false;
    }
    return true;
}

bool OrderBook::canMatchLimitOrder(const Side side, const Price price) {
    if (side == Side::Buy) {
        if (d_asks.empty()) {
            return false;
        }
        return price >= d_asks.begin()->first;
    } else {
        if (d_bids.empty()) {
            return false;
        }
        return price <= d_bids.begin()->first ;
    }
}

bool OrderBook::canMatchMarketOrder(const Side side) {
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
        d_orderMap[orderId] = {side, price, std::prev(d_bids[price].end())};
    } else {
        d_asks[price].push_back(order);
        d_orderMap[orderId] = {side, price, std::prev(d_asks[price].end())};
    }

    std::vector<Trade> trades = matchExistingOrders();

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

            trades.emplace_back(bidOrderId, askOrderId, tradePrice, quantityToFill);
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
    auto [side, price, listIter] = d_orderMap[orderId];
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
    std::scoped_lock<std::mutex> ordersLock(d_ordersMutex);
    
    const OrderId orderId = d_nextOrderId++;
    Order order(orderId, orderType, side, price, initialQuantity);

    return shouldAddLimitOrder(orderId, orderType, side, price) ? addOrder(order) : std::vector<Trade>();
}

std::vector<Trade> OrderBook::createAddMarketOrder(const Side side, const Quantity quantity) {
    std::scoped_lock<std::mutex> ordersLock(d_ordersMutex);

    const OrderId orderId = d_nextOrderId++;
    Order order(orderId, OrderType::Market, side, std::nullopt, quantity);

    if (!canMatchMarketOrder(side)) {
        return {};
    }

    order.setPrice(decideMarketOrderPrice(side));
    return addOrder(order);
}

void OrderBook::cancelOrder(const OrderId orderId) {
    std::scoped_lock<std::mutex> ordersLock(d_ordersMutex);

    if (!d_orderMap.contains(orderId)) {
        std::cerr << "Cannot find order. Cancellation failed. orderId=" << orderId << std::endl;
        return;
    }

    cancelExistingOrder(orderId);
}

} // namespace order_book