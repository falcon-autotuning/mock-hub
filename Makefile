.PHONY: all build test update-hashes archive clean help

VERSION := 1.0.0
REPO_NAME := mock-hub
PKG_NAME := mockHub
SO_NAME := build/mock-hub-wrapper.so
CPP_SRC := mock-hub-wrapper.cpp
FAL_FILE := mockHub.fal
YML_FILE := falcon.yml
TARBALL := $(PKG_NAME)-$(VERSION).tar.gz

VCPKG_DIR ?= $(CURDIR)/../std-lib/vcpkg_installed/x64-linux-dynamic
FALCON_PREFIX ?= /opt/falcon

CXX ?= clang++
CXXFLAGS := -std=c++20 -O3 -fPIC -Wall -Wextra
INCLUDES := -I$(VCPKG_DIR)/include -I$(FALCON_PREFIX)/include
LDFLAGS := -L$(VCPKG_DIR)/lib -L$(FALCON_PREFIX)/lib -lfalcon-core -lfalcon-typing -lfalcon-routine -lfalcon-database -lfalcon-comms -lnats -lspdlog -lfmt -lhdf5_cpp -lhdf5 -lyaml-cpp

help: ## Show available targets
	@echo "MockHub Package"
	@echo "==============="
	@echo "Version: $(VERSION)"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

all: build ## Build wrapper library

build: ## Compile C++ FFI wrapper
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -shared -o $(SO_NAME) $(CPP_SRC) $(INCLUDES) $(LDFLAGS)
	@echo "✓ Built $(SO_NAME)"

update-hashes: build ## Update SHA-256 hash in falcon.yml
	@HASH=$$(sha256sum $(SO_NAME) | awk '{print $$1}') && \
	sed -i "s|build/mock-hub-wrapper.so: sha256:.*|build/mock-hub-wrapper.so: sha256:$$HASH|" $(YML_FILE) && \
	echo "✓ Updated $(YML_FILE) with hash $$HASH"

test: build ## Run self tests
	@cd tests && LD_LIBRARY_PATH=$(VCPKG_DIR)/lib:$(FALCON_PREFIX)/lib:$$LD_LIBRARY_PATH \
	$(VCPKG_DIR)/bin/falcon-test ./run_tests.fal --log-level info

archive: update-hashes ## Create package tarball
	@mkdir -p dist
	tar -czvf dist/$(TARBALL) $(YML_FILE) $(FAL_FILE) $(SO_NAME) README.md
	@echo "✓ Created dist/$(TARBALL)"

clean: ## Remove build artifacts
	rm -rf build dist .falcon
	@echo "✓ Clean complete"
