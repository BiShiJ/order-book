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

namespace cr = std::chrono;

class OrderBook {
  private:
    /**
     * Private struct definitions
     */

    struct LocalTimeInfo {
        cr::local_time<cr::system_clock::duration> localTime;
        cr::local_days localDay;
        cr::local_seconds localSecond;
        cr::seconds secondsWithinDay;
    };

    struct MarketTimeStatus {
        cr::time_point<cr::system_clock> timePoint;
        bool isMarketOpen;

        MarketTimeStatus(const cr::time_point<cr::system_clock>& time) :
            timePoint(time), isMarketOpen(isMarketInOpenHours(time)) {}
        MarketTimeStatus(const cr::time_point<cr::system_clock>& time, bool isOpen) :
            timePoint(time), isMarketOpen(isOpen) {}
    };

    struct OrderLocation {
        Side side{};
        Price price{};
        std::list<Order>::iterator listIter;
    };

    /**
     * Static members
     */
    
    static cr::seconds s_marketOpenTime;
    static cr::seconds s_marketCloseTime;

    static bool isMarketInOpenHours(const cr::time_point<cr::system_clock>& timePoint);
    static LocalTimeInfo getLocalTimeInfo(const cr::time_point<cr::system_clock>& timePoint);
    static bool isWeekday(const cr::local_days& localDay);
    static cr::local_days calculateNextWeekday(const cr::local_days& localDay);
    static cr::time_point<cr::system_clock> calculateNextOpenTime(const cr::time_point<cr::system_clock>& timePoint);
    static cr::time_point<cr::system_clock> calculateNextCloseTime(const cr::time_point<cr::system_clock>& timePoint);
    static cr::time_point<cr::system_clock> calculateNextEventTime(
        bool isNextEventOpen, const cr::time_point<cr::system_clock>& timePoint);

    /**
     * Thread-safe member variable
     */
    std::atomic<MarketTimeStatus> d_marketTimeStatus = MarketTimeStatus(cr::system_clock::now());

    /**
     * Use @c d_ordersMutex to protect order states
     */
    std::mutex d_ordersMutex;
    OrderId d_nextOrderId = OrderId(1);
    std::map<Price, std::list<Order>, std::greater<>> d_bids;
    std::map<Price, std::list<Order>, std::less<>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;
    std::map<OrderId, Order> d_pendingLimitOrders;

    /**
     * Use @c d_marketMutex to protect market states
     */
    std::mutex d_marketMutex;
    std::condition_variable d_marketConditionVariable;
    bool d_isShuttingDown = false;

    /**
     * Thread handling market open/close
     */
    std::thread d_marketStatusThread;

    /**
     * Market open/close logic
     */
    void openCloseMarket();
    void onMarketOpen(const cr::time_point<cr::system_clock>& timePoint);
    void addPendingLimitOrders();
    void onMarketClose(const cr::time_point<cr::system_clock>& timePoint);
    void cancelRemainingDayOrders();

    /**
     * Limit order logic
     */
    std::vector<Trade> addLimitOrder(Order& order);
    bool shouldAddLimitOrder(OrderId orderId, OrderType orderType, Side side, Price price) const;
    bool canMatchLimitOrder(Side side, Price price) const;

    /**
     * Market order logic
     */
    bool canMatchMarketOrder(Side side) const;
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
        std::scoped_lock marketLock(d_marketMutex);
        d_isShuttingDown = true;
        d_marketConditionVariable.notify_all();
    }

    d_marketStatusThread.join();
}

} // namespace order_book