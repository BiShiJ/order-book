// order_book.h                                                                                                -*-C++-*-
#pragma once

//@PURPOSE: Provide a matching engine for limit and market orders with scheduled market session behavior.
//
//@CLASSES:
//  OrderBook: central limit order book with session and matching logic
//
//@MACROS:
//
//@DESCRIPTION: The book maintains bid and ask levels, matches crossing orders,
// and runs a background thread to open and close the market on a weekday schedule.
// Public entry points synchronize access to in-memory structures; refer to class-level notes for threading.

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

/// Mechanism class implementing a central limit order book with market-hours handling.
///
/// Thread safety: public methods that mutate or read book state use `d_ordersMutex`
/// or atomic market status as documented at each function.
/// The destructor coordinates shutdown with the market thread.
class OrderBook {
  private:
    /// Local calendar and clock parts for session scheduling.
    struct LocalTimeInfo {
        cr::local_time<cr::system_clock::duration> localTime;
        cr::local_days localDay;
        cr::local_seconds localSecond;
        cr::seconds secondsWithinDay;
    };

    /// Snapshot of system time and whether the session is open.
    struct MarketTimeStatus {
        cr::time_point<cr::system_clock> timePoint;
        bool isMarketOpen;

        MarketTimeStatus(const cr::time_point<cr::system_clock>& time) :
            timePoint(time), isMarketOpen(isMarketInOpenHours(time)) {}
        MarketTimeStatus(const cr::time_point<cr::system_clock>& time, bool isOpen) :
            timePoint(time), isMarketOpen(isOpen) {}
    };

    /// Iterator and side for locating an order in the price-level lists.
    struct OrderLocation {
        Side side{};
        Price price{};
        std::list<Order>::iterator listIter;
    };

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

    /// Last known market open/close state, updated by the market thread and readers.
    std::atomic<MarketTimeStatus> d_marketTimeStatus = MarketTimeStatus(cr::system_clock::now());

    /// Mutex protecting order containers and matching.
    std::mutex d_ordersMutex;
    OrderId d_nextOrderId = OrderId(1);
    std::map<Price, std::list<Order>, std::greater<>> d_bids;
    std::map<Price, std::list<Order>, std::less<>> d_asks;
    std::unordered_map<OrderId, OrderLocation> d_orderMap;
    std::map<Price, Quantity, std::greater<>> d_bidVolumes;
    std::map<Price, Quantity, std::less<>> d_askVolumes;
    std::map<OrderId, Order> d_pendingLimitOrders;

    /// Mutex and condition variable for market thread wakeups and shutdown.
    std::mutex d_marketMutex;
    std::condition_variable d_marketConditionVariable;
    bool d_isShuttingDown = false;

    /// Joining thread that transitions market open/close on schedule.
    std::jthread d_marketStatusThread;

    void openCloseMarket();
    void onMarketOpen(const cr::time_point<cr::system_clock>& timePoint);
    void addPendingLimitOrders();
    void onMarketClose(const cr::time_point<cr::system_clock>& timePoint);
    void cancelRemainingDayOrders();

    std::vector<Trade> addLimitOrder(Order& order);
    bool shouldAddLimitOrder(const Order& order) const;
    bool canMatchLimitOrder(Side orderSide, Price orderPrice) const;
    bool canFullyFillLimitOrder(const Order& order) const;

    bool canMatchMarketOrder(Side orderSide) const;
    Price decideMarketOrderPrice(Side orderSide);

    std::vector<Trade> addOrder(Order& order);
    std::vector<Trade> matchExistingOrders();
    void eraseBestPriceLevelWhenEmpty();
    void cancelRemainingQuantityAfterMatching(OrderId orderId, OrderType orderType);

    /// @pre `orderId` must already exist in the order book.
    void cancelExistingOrder(OrderId orderId);

  public:
    /// Create a book and start the market-hours thread.
    OrderBook();

    /// Disable copying and moving
    OrderBook(const OrderBook& other) = delete;
    OrderBook& operator=(const OrderBook& other) = delete;
    OrderBook(const OrderBook&& other) noexcept = delete;
    OrderBook& operator=(const OrderBook&& other) = delete;

    ~OrderBook();

    /// Create a limit order and return any trades produced while holding `d_ordersMutex`.
    /// If the market is closed, defer the order until open; returns empty in that case.
    std::vector<Trade> createAddLimitOrder(OrderType orderType, Side side, Price price, Quantity initialQuantity);

    /// Create a market order when the market is open; return trades or empty if none.
    std::vector<Trade> createAddMarketOrder(Side side, Quantity initialQuantity);

    /// Cancel a resting order by id; no effect if the id is unknown.
    void cancelOrder(OrderId orderId);
};

inline OrderBook::OrderBook() :
    d_marketStatusThread([this] { openCloseMarket(); }) {}

inline OrderBook::~OrderBook() {
    std::scoped_lock marketLock(d_marketMutex);
    d_isShuttingDown = true;
    d_marketConditionVariable.notify_all();
}

} // namespace order_book
