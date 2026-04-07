// main.cpp                                                                                                    -*-C++-*-

#include <iostream>

#include "order_book.h"

int main() {
    std::cout << "C++ version: ";
    if (__cplusplus == 202302L) {
        std::cout << "C++23";
    }
    else if (__cplusplus == 202002L) {
        std::cout << "C++20";
    }
    else if (__cplusplus == 201703L) {
        std::cout << "C++17";
    }
    else if (__cplusplus == 201402L) {
        std::cout << "C++14";
    }
    else if (__cplusplus == 201103L) {
        std::cout << "C++11";
    }
    else if (__cplusplus == 199711L) {
        std::cout << "C++98";
    }
    else {
        std::cout << "unknown (" << __cplusplus << ")";
    }
    std::cout << "\n";

    order_book::OrderBook orderBook;

    return 0;
}