// aliases.h                                                                                                   -*-C++-*-
#pragma once

//@PURPOSE: Provide vocabulary type aliases for order identifiers, prices, and quantities.
//
//@CLASSES:
//
//@MACROS:
//
//@DESCRIPTION: Each alias names an integral type used consistently across the order book component;
// see individual typedefs for intended semantics.

#include <cstdint>

namespace order_book {

/// Type alias intended as a unique order identifier.
using OrderId = std::uint64_t;

/// Type alias intended as a limit price in the book's price domain.
using Price = std::int32_t;

/// Type alias intended as a non-negative size or fill amount.
using Quantity = std::uint64_t;

} // namespace order_book
