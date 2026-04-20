# GNU Make finds this as 'makefile' (repo .gitignore ignores 'Makefile' for CMake).
CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -pedantic -pthread
# std::atomic<OrderBook::MarketTimeStatus> is 16 bytes; GCC may call __atomic_* from libatomic.
LDFLAGS ?= -pthread -latomic
SOURCES := main.cpp order_book.cpp
TARGET := order_book.out
CLANG_TIDY ?= clang-tidy

.PHONY: all build lint clean

.DEFAULT_GOAL := build

all: clean build lint

build: $(TARGET)

$(TARGET): $(SOURCES) order_book.h order.h trade.h enums.h aliases.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

lint:
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { echo "clang-tidy not found; install e.g. apt install clang-tidy or brew install llvm"; exit 1; }
	$(CLANG_TIDY) $(SOURCES) -- $(CXXFLAGS)

clean:
	rm -f $(TARGET)
