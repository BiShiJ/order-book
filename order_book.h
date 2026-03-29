#pragma once

#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "trade.h"

namespace order_book {

class OrderBook {
  private:
    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator listIter;
    };

    OrderId d_nextOrderId;
    
    std::map<Price, std::list<Order>, std::greater<Price>> d_bids;
    std::map<Price, std::list<Order>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;
    
    bool shouldAddLimitOrder(const OrderId orderId, const OrderType orderType, const Side side, const Price price);
    bool canMatchLimitOrder(const Side side, const Price price);
    bool canMatchMarketOrder(const Side);
    Price decideMarketOrderPrice(const Side);
    std::vector<Trade> addOrder(Order& order);
    std::vector<Trade> matchExistingOrders();
    void cancelRemainingQuantityAfterMatching(const OrderId orderId, const OrderType orderType);

    /// @pre orderId must exist in the order book
    void cancelExistingOrder(const OrderId orderId);

  public:
    OrderBook();
    std::vector<Trade> createAddLimitOrder(
        const OrderType orderType, const Side side, const Price price, const Quantity initialQuantity);
    std::vector<Trade> createAddMarketOrder(const Side side, const Quantity initialQuantity);
    void cancelOrder(const OrderId orderId);
};

inline OrderBook::OrderBook() :
    d_nextOrderId(OrderId(1)), d_bids(), d_asks(), d_orderMap() {}

} // namespace order_book