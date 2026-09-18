.PHONY: help configure build test update-hashes archive clean vcpkg-bootstrap

VERSION := 1.0.0
PKG_NAME := mockHub
SO_NAME := build/mock-hub-wrapper.so
FAL_FILE := mockHub.fal
YML_FILE := falcon.yml
TARBALL := $(PKG_NAME)-$(VERSION).tar.gz

PRESET ?= linux-clang-release
VCPKG_DIR ?= $(CURDIR)/vcpkg_installed/x64-linux-dynamic
FALCON_PREFIX ?= /opt/falcon

help: ## Show available targets
	@echo "MockHub Package"
	@echo "==============="
	@echo "Version: $(VERSION)"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

vcpkg-bootstrap: ## Bootstrap vcpkg and install dependencies
	@echo "Bootstrapping vcpkg..."
	MAKELEVEL=0 cmake -P cmake/bootstrap/bootstrap-vcpkg.cmake

configure: vcpkg-bootstrap ## Configure CMake with preset
	@echo "Configuring $(PRESET)..."
	MAKELEVEL=0 cmake --preset $(PRESET)

all: build ## Build wrapper library

build: configure ## Build wrapper library using CMake and vcpkg
	@echo "Building $(PRESET)..."
	cmake --build --preset $(PRESET)

update-hashes: build ## Update SHA-256 hash in falcon.yml
	@python3 scripts/update_hashes.py $(CURDIR)

test: build ## Run self tests
	@cd tests && LD_LIBRARY_PATH=$(VCPKG_DIR)/lib:$(FALCON_PREFIX)/lib:$$LD_LIBRARY_PATH \
	$(VCPKG_DIR)/bin/falcon-test ./run_tests.fal --log-level info

archive: update-hashes ## Create package tarball
	@mkdir -p dist
	tar -czvf dist/$(TARBALL) $(YML_FILE) $(FAL_FILE) $(SO_NAME) README.md
	@echo "✓ Created dist/$(TARBALL)"

clean: ## Remove build artifacts
	rm -rf build vcpkg_installed dist .falcon
	@echo "✓ Clean complete"
