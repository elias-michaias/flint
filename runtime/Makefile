# Makefile for Flint Runtime System
# Supports Curry-style functional logic programming with narrowing

# ==============================================================================
# CROSS-PLATFORM DETECTION AND TOOLCHAIN CONFIGURATION
# ==============================================================================

# Detect platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
    CC ?= clang
    AR = ar
    RANLIB = ranlib
    SHARED_EXT = .dylib
    STATIC_EXT = .a
    # macOS-specific flags for dead code elimination
    LDFLAGS_DEAD_CODE = -Wl,-dead_strip
    # Use native optimization
    ARCH_FLAGS = -march=native
else ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    CC ?= gcc
    AR = ar
    RANLIB = ranlib
    SHARED_EXT = .so
    STATIC_EXT = .a
    # Linux-specific flags for dead code elimination
    LDFLAGS_DEAD_CODE = -Wl,--gc-sections
    # Use native optimization
    ARCH_FLAGS = -march=native
else ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    PLATFORM = windows
    CC ?= gcc
    AR = ar
    RANLIB = ranlib
    SHARED_EXT = .dll
    STATIC_EXT = .a
    # Windows/MinGW flags for dead code elimination
    LDFLAGS_DEAD_CODE = -Wl,--gc-sections
    ARCH_FLAGS = -march=native
else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
    PLATFORM = windows
    CC ?= gcc
    AR = ar
    RANLIB = ranlib
    SHARED_EXT = .dll
    STATIC_EXT = .a
    # Windows/Cygwin flags for dead code elimination
    LDFLAGS_DEAD_CODE = -Wl,--gc-sections
    ARCH_FLAGS = -march=native
else
    PLATFORM = unknown
    CC ?= gcc
    AR = ar
    RANLIB = ranlib
    SHARED_EXT = .so
    STATIC_EXT = .a
    LDFLAGS_DEAD_CODE = -Wl,--gc-sections
    ARCH_FLAGS = -march=native
endif

# Compiler and build configuration
CFLAGS = -Wall -Wextra -std=c99 -O2 -g $(ARCH_FLAGS)
# Add function/data sections for dead code elimination
CFLAGS += -ffunction-sections -fdata-sections
INCLUDES = -I. -I$(LIBDILL_INSTALL_DIR)/include -I$(AMOEBA_DIR)
LDFLAGS = -lm $(LIBDILL_LIBRARY) $(LDFLAGS_DEAD_CODE)

# ==============================================================================
# DEPENDENCY CONFIGURATION - All dependency settings in one place
# ==============================================================================

# libdill - Structured concurrency and async operations
LIBDILL_NAME = libdill
LIBDILL_URL = http://libdill.org/libdill-2.14.tar.gz
LIBDILL_PURPOSE = Structured concurrency and async operations
LIBDILL_VERSION = 2.14
LIBDILL_DIR = lib/libdill
LIBDILL_BUILD_DIR = $(LIBDILL_DIR)/libdill-$(LIBDILL_VERSION)
LIBDILL_INSTALL_DIR = $(LIBDILL_DIR)/libdill-install
LIBDILL_ARCHIVE = $(LIBDILL_DIR)/libdill-$(LIBDILL_VERSION).tar.gz
LIBDILL_LIBRARY = $(LIBDILL_INSTALL_DIR)/lib/libdill.a
LIBDILL_HEADERS = $(LIBDILL_INSTALL_DIR)/include/libdill.h

# amoeba - Cassowary constraint solver for linear programming  
AMOEBA_NAME = amoeba
AMOEBA_URL = https://raw.githubusercontent.com/starwing/amoeba/master/amoeba.h
AMOEBA_PURPOSE = Cassowary constraint solver for linear programming
AMOEBA_DIR = lib/amoeba
AMOEBA_HEADER = $(AMOEBA_DIR)/amoeba.h
AMOEBA_LICENSE_URL = https://raw.githubusercontent.com/starwing/amoeba/master/LICENSE
AMOEBA_TEST_URL = https://raw.githubusercontent.com/starwing/amoeba/master/test.c

# Dependency status files (to track what's been built)
DEPS_STATUS_DIR = .deps
LIBDILL_STATUS = $(DEPS_STATUS_DIR)/libdill.done
AMOEBA_STATUS = $(DEPS_STATUS_DIR)/amoeba.done

# Directories
OBJDIR = object
TESTDIR = test
OUTDIR = out

# Source files
SOURCES = runtime.c unification.c narrowing.c environment.c matching.c constraint.c linear.c interop.c async.c list.c
HEADERS = types.h runtime.h

# Object files (in object directory)
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:.c=.o))

# Library name (in out directory)
LIBRARY = $(OUTDIR)/libflint_runtime.a

# Temporary directory for extracting libdill objects
LIBDILL_EXTRACT_DIR = $(OUTDIR)/libdill_objects

# Test programs (all .c files in test directory)
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_PROGRAMS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(TESTDIR)/%)

.PHONY: all clean test install dirs deps check-deps list-deps clean-deps clean-all platform-info

all: deps dirs $(LIBRARY)

# ==============================================================================
# PLATFORM AND BUILD INFORMATION
# ==============================================================================

platform-info:
	@echo "🔧 Platform and Build Information:"
	@echo "  Platform: $(PLATFORM) ($(UNAME_S) $(UNAME_M))"
	@echo "  Compiler: $(CC)"
	@echo "  Archiver: $(AR)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
	@echo "  Dead code elimination: $(LDFLAGS_DEAD_CODE)"
	@echo "  Architecture: $(ARCH_FLAGS)"
	@echo ""

# ==============================================================================
# DEPENDENCY MANAGEMENT
# ==============================================================================

deps: check-deps $(LIBDILL_STATUS) $(AMOEBA_STATUS)
	@echo "✅ All dependencies ready"

check-deps:
	@echo "📋 Dependencies required for Flint Runtime:"
	@echo "  - $(LIBDILL_NAME): $(LIBDILL_PURPOSE)"
	@echo "  - $(AMOEBA_NAME): $(AMOEBA_PURPOSE)"
	@echo ""

list-deps:
	@echo "📦 Configured dependencies:"
	@echo "  $(LIBDILL_NAME): $(LIBDILL_PURPOSE)"
	@echo "    URL: $(LIBDILL_URL)"
	@echo "    Status: $$(if [ -f $(LIBDILL_STATUS) ]; then echo '✅ Ready'; else echo '❌ Not installed'; fi)"
	@echo ""
	@echo "  $(AMOEBA_NAME): $(AMOEBA_PURPOSE)"
	@echo "    URL: $(AMOEBA_URL)"
	@echo "    Status: $$(if [ -f $(AMOEBA_STATUS) ]; then echo '✅ Ready'; else echo '❌ Not installed'; fi)"
	@echo ""

# libdill dependency - structured concurrency library
$(LIBDILL_STATUS): $(LIBDILL_LIBRARY)
	@mkdir -p $(DEPS_STATUS_DIR)
	@echo "libdill $(LIBDILL_VERSION) built and installed" > $(LIBDILL_STATUS)

$(LIBDILL_LIBRARY): $(LIBDILL_ARCHIVE)
	@echo "📥 Building libdill (structured concurrency)..."
	@if [ ! -d $(LIBDILL_BUILD_DIR) ]; then \
		echo "  Extracting libdill..."; \
		cd $(LIBDILL_DIR) && tar -xzf libdill-$(LIBDILL_VERSION).tar.gz; \
	fi
	@if [ ! -f $(LIBDILL_LIBRARY) ]; then \
		echo "  Configuring and building libdill..."; \
		cd $(LIBDILL_BUILD_DIR) && \
		./configure --prefix=$(PWD)/$(LIBDILL_INSTALL_DIR) --enable-static --disable-shared && \
		make -j$$(nproc 2>/dev/null || echo 4) && \
		make install; \
		echo "  ✅ libdill built and installed"; \
	fi

$(LIBDILL_ARCHIVE):
	@echo "📥 Downloading libdill $(LIBDILL_VERSION)..."
	@mkdir -p $(LIBDILL_DIR)
	@curl -L --fail --silent --show-error -o $(LIBDILL_ARCHIVE) $(LIBDILL_URL)
	@echo "  ✅ libdill downloaded"

# amoeba dependency - header-only Cassowary constraint solver
$(AMOEBA_STATUS): $(AMOEBA_HEADER)
	@mkdir -p $(DEPS_STATUS_DIR)
	@echo "amoeba header-only library downloaded" > $(AMOEBA_STATUS)

$(AMOEBA_HEADER):
	@echo "📥 Downloading amoeba (Cassowary constraint solver)..."
	@mkdir -p $(AMOEBA_DIR)
	@curl -L --fail --silent --show-error -o $(AMOEBA_HEADER) $(AMOEBA_URL)
	@curl -L --fail --silent --show-error -o $(AMOEBA_DIR)/LICENSE $(AMOEBA_LICENSE_URL) || echo "  ⚠️  Could not download LICENSE"
	@curl -L --fail --silent --show-error -o $(AMOEBA_DIR)/test.c $(AMOEBA_TEST_URL) || echo "  ⚠️  Could not download test.c"
	@echo "  ✅ amoeba header-only library downloaded"

dirs:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(TESTDIR)
	@mkdir -p $(OUTDIR)
	@mkdir -p $(LIBDILL_EXTRACT_DIR)

$(LIBRARY): $(OBJECTS) $(LIBDILL_LIBRARY)
	@echo "📦 Creating self-contained libflint_runtime.a with embedded libdill..."
	@# Extract libdill objects to temporary directory
	@cd $(LIBDILL_EXTRACT_DIR) && ar x ../../$(LIBDILL_LIBRARY)
	@# Create the library with both Flint and libdill objects
	ar rcs $@ $(OBJECTS) $(LIBDILL_EXTRACT_DIR)/*.o
	ranlib $@
	@echo "✅ Self-contained library created: $(LIBRARY)"

# All object files depend on dependencies being ready first
$(OBJECTS): | $(LIBDILL_STATUS) $(AMOEBA_STATUS)

$(OBJDIR)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_PROGRAMS)
	@for test in $(TEST_PROGRAMS); do \
		echo "Running $$test..."; \
		./$$test; \
	done

$(TESTDIR)/%: $(TESTDIR)/%.c $(LIBRARY)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OUTDIR) -lflint_runtime $(LDFLAGS) -o $@

install: $(LIBRARY)
	cp $(LIBRARY) /usr/local/lib/
	cp $(HEADERS) /usr/local/include/

clean:
	rm -f $(OBJECTS) $(LIBRARY) $(TEST_PROGRAMS)
	rm -f $(OBJDIR)/*.o
	rm -f $(TESTDIR)/*.o
	rm -rf $(OUTDIR)/*
	rm -rf $(LIBDILL_EXTRACT_DIR)

clean-deps:
	@echo "🧹 Cleaning dependencies..."
	rm -rf $(LIBDILL_DIR)
	rm -rf $(AMOEBA_DIR)
	rm -rf $(DEPS_STATUS_DIR)
	@echo "✅ Dependencies cleaned"

clean-all: clean clean-deps
	@echo "🧹 Complete cleanup finished"

# Development targets
debug: CFLAGS += -DDEBUG -O0
debug: dirs $(LIBRARY)

profile: CFLAGS += -pg
profile: dirs $(LIBRARY)

# Dependencies
$(OBJDIR)/runtime.o: runtime.c runtime.h types.h
$(OBJDIR)/unification.o: unification.c runtime.h types.h  
$(OBJDIR)/narrowing.o: narrowing.c runtime.h types.h
$(OBJDIR)/environment.o: environment.c runtime.h types.h
$(OBJDIR)/matching.o: matching.c runtime.h types.h
$(OBJDIR)/constraint.o: constraint.c runtime.h types.h
$(OBJDIR)/linear.o: linear.c runtime.h types.h
$(OBJDIR)/interop.o: interop.c runtime.h types.h
$(OBJDIR)/async.o: async.c runtime.h types.h
