# Compiler and flags
CXX       ?= g++
CXXFLAGS  += -O2 -Wall -Wextra -pedantic -std=c++17
# Try opencv4 first; if that fails user can override with: make PKG=cv -- etc.
PKG       ?= opencv4
OPENCV_CFLAGS := $(shell pkg-config --cflags $(PKG) 2>/dev/null)
OPENCV_LIBS   := $(shell pkg-config --libs $(PKG) 2>/dev/null)

ifeq (,$(OPENCV_CFLAGS))
  $(warning Could not get OpenCV cflags via pkg-config for '$(PKG)'. If build fails, install OpenCV dev package or set PKG=opencv4/opencv)
endif

SRC       := extract_color_channel.cpp
BUILD_DIR := build
BIN       := $(BUILD_DIR)/extract_color_channel

.PHONY: all clean help

all: $(BIN)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN): $(SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPENCV_CFLAGS) $< -o $@ $(OPENCV_LIBS)
	@echo "Built $@"

clean:
	@rm -f $(BIN)
	@echo "Cleaned $(BIN)"

help:
	@echo "Targets:" \
	      "\n  all     : Build the binary (default)" \
	      "\n  clean   : Remove built binary" \
	      "\n  help    : This help" \
	      "\nVariables:" \
	      "\n  PKG     : pkg-config module (default opencv4)" \
	      "\nExamples:" \
	      "\n  make -C src" \
	      "\n  ./src/build/extract_color_channel <input> <output> <channel>"
