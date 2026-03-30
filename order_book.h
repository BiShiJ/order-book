#pragma once

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <thread>
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

    /**
     * Internal states that should be protected by mutex
     */
    OrderId d_nextOrderId;
    std::map<Price, std::list<Order>, std::greater<Price>> d_bids;
    std::map<Price, std::list<Order>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;

    /**
     * Multithreading support for internal states
     */
    std::mutex d_ordersMutex;

    /**
     * Manage Day Order auto cancellation on market close
     */

    std::atomic<bool> d_isMarketOpen;
    std::thread d_dayOrderThread;
    
    void autoCancelDayOrders();

    /**
     * Limit order logic
     */
    bool shouldAddLimitOrder(const OrderId orderId, const OrderType orderType, const Side side, const Price price);
    bool canMatchLimitOrder(const Side side, const Price price);

    /**
     * Market order logic
     */
    bool canMatchMarketOrder(const Side side);
    Price decideMarketOrderPrice(const Side);

    /**
     * Internal logic to manipulate orders
     */
    
    std::vector<Trade> addOrder(Order& order);
    std::vector<Trade> matchExistingOrders();
    void cancelRemainingQuantityAfterMatching(const OrderId orderId, const OrderType orderType);

    /// @pre @c orderId must already exist in the order book.
    void cancelExistingOrder(const OrderId orderId);

  public:
    OrderBook();
    ~OrderBook();

    /**
     * Thread-safe public operations
     */
    std::vector<Trade> createAddLimitOrder(
        const OrderType orderType, const Side side, const Price price, const Quantity initialQuantity);
    std::vector<Trade> createAddMarketOrder(const Side side, const Quantity initialQuantity);
    void cancelOrder(const OrderId orderId);
};

inline OrderBook::OrderBook() :
    d_nextOrderId(OrderId(1)),
    d_bids(),
    d_asks(),
    d_orderMap(),
    d_isMarketOpen(true),
    d_dayOrderThread([this] { autoCancelDayOrders(); }) {}

inline OrderBook::~OrderBook() {
    d_isMarketOpen.store(false, std::memory_order_release);
    d_dayOrderThread.join();
}

} // namespace order_book