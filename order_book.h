#pragma once

#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "order.h"
#include "trade.h"

namespace order_book {

class OrderBook {
  private:
    using OrderLocation = std::list<Order>::iterator;

    OrderId d_nextOrderId;
    
    std::map<Price, std::list<Order>, std::greater<Price>> d_bids;
    std::map<Price, std::list<Order>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;
    
    std::vector<Trade> MatchOrders();

  public:
    OrderBook();
};

inline OrderBook::OrderBook() :
    d_nextOrderId(OrderId(1)), d_bids(), d_asks(), d_orderMap() {}

} // namespace order_book