#include "order_book.h"

#include <iostream>

#include "order.h"

namespace order_book {

bool OrderBook::canMatch(const Side side, const Price price) {
    if (side == Side::BUY) {
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

            // Use passive order's price as trade price
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

std::vector<Trade> OrderBook::createAddOrder(
    const OrderType orderType, const Side side, const Price price, const Quantity initialQuantity) {
    const OrderId orderId = d_nextOrderId++;
    Order order {orderId, orderType, side, price, initialQuantity};

    if (orderType == OrderType::IMMEDIATE_OR_CANCELLED && !canMatch(side, price)) {
        std::cout << "No satisfying order existed for this IMMEDIATE_OR_CANCELLED order."
                  << " orderId=" << orderId
                  << " side=" << side
                  << " price=" << price
                  << std::endl;
        return {};
    }

    if (side == Side::BUY) {
        d_bids[price].push_back(order);
        d_orderMap[orderId] = {side, price, prev(d_bids[price].end())};
    } else {
        d_asks[price].push_back(order);
        d_orderMap[orderId] = {side, price, prev(d_asks[price].end())};
    }

    std::vector<Trade> trades = matchExistingOrders();

    if (orderType == OrderType::IMMEDIATE_OR_CANCELLED && d_orderMap.contains(orderId)) {
        cancelOrder(orderId);
    }

    return trades;
}

void OrderBook::cancelOrder(const OrderId orderId) {
    if (!d_orderMap.contains(orderId)) {
        std::cerr << "Cannot find order. Cancellation failed. orderId=" << orderId << std::endl;
        return;
    }

    auto [side, price, listIter] = d_orderMap[orderId];
    d_orderMap.erase(orderId);

    if (side == Side::BUY) {
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

} // namespace order_book