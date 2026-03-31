#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <unordered_map>
#include <vector>

#include "aliases.h"
#include "order.h"
#include "trade.h"

namespace order_book {

using namespace std::chrono;

class OrderBook {
  private:
    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator listIter;
    };

    static bool isMarketInOpenHours();

    std::atomic<bool> d_isMarketOpen{isMarketInOpenHours()};

    /**
     * Use @c d_ordersMutex to protect order states
     */
    std::mutex d_ordersMutex;
    OrderId d_nextOrderId = OrderId(1);
    std::map<Price, std::list<Order>, std::greater<Price>> d_bids;
    std::map<Price, std::list<Order>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;
    std::map<OrderId, Order> d_pendingLimitOrders;

    /**
     * Use @c d_marketMutex to protect market states
     */
    std::mutex d_marketMutex;
    std::condition_variable d_marketConditionVariable;
    bool d_isShuttingDown = false;

    std::thread d_marketStatusThread;

    void openCloseMarket();
    time_point<system_clock> calculateNextOpenTime();
    time_point<system_clock> calculateNextCloseTime();

    void onMarketOpen();
    void addPendingLimitOrders();
    void onMarketClose();
    void cancelRemainingDayOrders();

    /**
     * Limit order logic
     */
    std::vector<Trade> addLimitOrder(Order& order);
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
    d_marketStatusThread([this] { openCloseMarket(); }) {}

inline OrderBook::~OrderBook() {
    {
        std::scoped_lock<std::mutex> marketLock(d_marketMutex);
        d_isShuttingDown = true;
        d_marketConditionVariable.notify_all();
    }

    d_marketStatusThread.join();
}

} // namespace order_book