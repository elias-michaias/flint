# Makefile for Flint Runtime System
# Supports Curry-style functional logic programming with narrowing

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -I.
LDFLAGS = 

# Directories
OBJDIR = object
TESTDIR = test
OUTDIR = out

# Source files
SOURCES = runtime.c unification.c narrowing.c environment.c pattern_matching.c
HEADERS = types.h runtime.h

# Object files (in object directory)
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:.c=.o))

# Library name (in out directory)
LIBRARY = $(OUTDIR)/libflint_runtime.a

# Test programs (all .c files in test directory)
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_PROGRAMS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(TESTDIR)/%)

.PHONY: all clean test install dirs

all: dirs $(LIBRARY)

dirs:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(TESTDIR)
	@mkdir -p $(OUTDIR)

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^
	ranlib $@

$(OBJDIR)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_PROGRAMS)
	@for test in $(TEST_PROGRAMS); do \
		echo "Running $$test..."; \
		./$$test; \
	done

$(TESTDIR)/%: $(TESTDIR)/%.c $(LIBRARY)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OUTDIR) -lflint_runtime -o $@

install: $(LIBRARY)
	cp $(LIBRARY) /usr/local/lib/
	cp $(HEADERS) /usr/local/include/

clean:
	rm -f $(OBJECTS) $(LIBRARY) $(TEST_PROGRAMS)
	rm -f $(OBJDIR)/*.o
	rm -f $(TESTDIR)/*.o
	rm -f $(OUTDIR)/*

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
$(OBJDIR)/pattern_matching.o: pattern_matching.c runtime.h types.h
