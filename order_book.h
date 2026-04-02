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
        Side side{};
        Price price{};
        std::list<Order>::iterator listIter;
    };

    /**
     * Static members
     */

    static bool isMarketInOpenHours();
    
    static time_point<system_clock> calculateNextOpenTime();
    static time_point<system_clock> calculateNextCloseTime();

    std::atomic<bool> d_isMarketOpen{isMarketInOpenHours()};

    /**
     * Use @c d_ordersMutex to protect order states
     */
    std::mutex d_ordersMutex;
    OrderId d_nextOrderId = OrderId(1);
    std::map<Price, std::list<Order>, std::greater<>> d_bids;
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

    void onMarketOpen();
    void addPendingLimitOrders();
    void onMarketClose();
    void cancelRemainingDayOrders();

    /**
     * Limit order logic
     */
    std::vector<Trade> addLimitOrder(Order& order);
    bool shouldAddLimitOrder(OrderId orderId, OrderType orderType, Side side, Price price);
    bool canMatchLimitOrder(Side side, Price price);

    /**
     * Market order logic
     */
    bool canMatchMarketOrder(Side side);
    Price decideMarketOrderPrice(Side side);

    /**
     * Internal logic to manipulate orders
     */
    
    std::vector<Trade> addOrder(Order& order);
    std::vector<Trade> matchExistingOrders();
    void cancelRemainingQuantityAfterMatching(OrderId orderId, OrderType orderType);

    /// @pre @c orderId must already exist in the order book.
    void cancelExistingOrder(OrderId orderId);

  public:
    OrderBook();

    // Disable copying and moving
    OrderBook(const OrderBook& other) = delete;
    OrderBook& operator=(const OrderBook& other) = delete;
    OrderBook(const OrderBook&& other) noexcept = delete;
    OrderBook& operator=(const OrderBook&& other) = delete;

    ~OrderBook();

    /**
     * Thread-safe public operations
     */
    std::vector<Trade> createAddLimitOrder(OrderType orderType, Side side, Price price, Quantity initialQuantity);
    std::vector<Trade> createAddMarketOrder(Side side, Quantity initialQuantity);
    void cancelOrder(OrderId orderId);
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