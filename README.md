# Order Book
A C++ order book implementation with basic matching and a market open/close simulation.

## Core Features

### Supported Order Types
- `Limit` (`GoodTilCanceled`): rests at its price until it trades (fills) or is explicitly cancelled.
- `Limit` (`ImmediateOrCancel`): only accepted if it can match immediately; any unfilled remainder is cancelled after matching.
- `Limit` (`Day`): rests until the market closes; any remaining quantity is cancelled at close.
- `Market`: executes immediately by matching against the current book using a synthetic limit price; any unfilled remainder is cancelled after matching.
- Both `Buy` and `Sell` sides are supported.

### Market Open/Close Mechanism
- During market close, `Market` orders are rejected.
- During market close, `Limit` orders are accepted but do not participate in matching until the next open.
- When the market closes, remaining `Day` limit orders are cancelled; non-`Day` resting limit orders remain in the book and can be cancelled manually.

### C++20 Compatibility
- Built with C++20 (`-std=c++20`, see `makefile`) and configured to use C++20-compatible toolchains/lint settings.
