# Makefile - golomb (no OpenCV dependency) and extract_color_channel (OpenCV)
# Usage:
#   make           # build both (default)
#   make golomb    # build only golomb (no OpenCV required)
#   make extract   # build only OpenCV example (requires OpenCV dev libs)
#   make clean     # clean

CXX       ?= g++
CXXFLAGS  += -O2 -Wall -Wextra -pedantic -std=c++17

# OpenCV pkg-config module (user can override: make extract PKG=opencv)
PKG       ?= opencv4
OPENCV_CFLAGS := $(shell pkg-config --cflags $(PKG) 2>/dev/null)
OPENCV_LIBS   := $(shell pkg-config --libs $(PKG) 2>/dev/null)

ifeq (,$(OPENCV_CFLAGS))
  $(warning Could not get OpenCV cflags via pkg-config for '$(PKG)'. If extract build fails, set PKG or install OpenCV dev package)
endif

SRCDIR    := src
BUILD_DIR := build

# --- Golomb (no external deps) ---
GOLOMB_SRCS := $(SRCDIR)/golomb.cpp $(SRCDIR)/golomb_main.cpp
GOLOMB_HDRS := $(SRCDIR)/golomb.hpp
GOLOMB_BIN  := $(BUILD_DIR)/golomb

# --- OpenCV example ---
EXTRACT_SRC := $(SRCDIR)/extract_color_channel.cpp
EXTRACT_BIN := $(BUILD_DIR)/extract_color_channel

.PHONY: all golomb extract image_transform clean help

all: golomb extract image_transform

# ensure build dir exists
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ---------------- Golomb target ----------------
# This target does NOT depend on OpenCV nor pkg-config; it only uses sources in src/.
$(GOLOMB_BIN): $(GOLOMB_SRCS) $(GOLOMB_HDRS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GOLOMB_SRCS) -o $@
	@echo "Built $@"

golomb: $(GOLOMB_BIN)

# ---------------- OpenCV extract target ----------------
# Uses OPENCV_CFLAGS / OPENCV_LIBS. If pkg-config didn't find OpenCV, you will see the earlier warning,
# but golomb target is unaffected.
$(EXTRACT_BIN): $(EXTRACT_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPENCV_CFLAGS) $< -o $@ $(OPENCV_LIBS)
	@echo "Built $@"

extract: $(EXTRACT_BIN)

# ---------------- image_transform (OpenCV) ----------------
IMAGE_SRC := $(SRCDIR)/image_transform.cpp
IMAGE_BIN := $(BUILD_DIR)/image_transform

$(IMAGE_BIN): $(IMAGE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OPENCV_CFLAGS) $< -o $@ $(OPENCV_LIBS)
	@echo "Built $@"

image_transform: $(IMAGE_BIN)

# ---------------- Cleanup ----------------
clean:
	@rm -f $(GOLOMB_BIN) $(EXTRACT_BIN) $(IMAGE_BIN)
	@rmdir --ignore-fail-on-non-empty $(BUILD_DIR) 2>/dev/null || true
	@echo "Cleaned build artifacts"

help:
	@echo "Targets:" \
	      "\n  all       : Build both binaries (default)" \
	      "\n  golomb    : Build only the golomb example (no OpenCV needed)" \
	      "\n  extract   : Build only the OpenCV example (requires OpenCV dev libs)" \
	      "\n  clean     : Remove built binaries" \
	      "\n  help      : Show this help" \
	      "\nVariables:" \
	      "\n  CXX       : C++ compiler (default g++)" \
	      "\n  PKG       : pkg-config module for OpenCV (default opencv4)" \
	      "\nExamples:" \
	      "\n  make" \
	      "\n  make golomb" \
	      "\n  make extract PKG=opencv" \
	      "\n  ./build/golomb"
