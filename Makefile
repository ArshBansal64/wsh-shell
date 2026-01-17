CC = gcc
CFLAGS-common = -std=gnu18 -Wall -Wextra -Werror -pedantic
CFLAGS = $(CFLAGS-common) -O2
CFLAGS-dbg = $(CFLAGS-common) -Og -ggdb
TARGET = wsh

# Source files
SRC = wsh.c dynamic_array.c utils.c hash_map.c

# Build directories
BUILDDIR = build
RELEASEDIR = $(BUILDDIR)/release
DEBUGDIR = $(BUILDDIR)/debug

# Object files
OBJ = $(patsubst %.c,$(RELEASEDIR)/%.o,$(SRC))
OBJ-dbg = $(patsubst %.c,$(DEBUGDIR)/%.o,$(SRC))

all: $(TARGET) $(TARGET)-dbg

# Optimized build
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Debug build
$(TARGET)-dbg: $(OBJ-dbg)
	$(CC) $(CFLAGS-dbg) $^ -o $@

# Compile release objects
$(RELEASEDIR)/%.o: %.c %.h | $(RELEASEDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile debug objects
$(DEBUGDIR)/%.o: %.c %.h | $(DEBUGDIR)
	$(CC) $(CFLAGS-dbg) -c $< -o $@

# Ensure build dirs exist
$(RELEASEDIR) $(DEBUGDIR):
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR) $(TARGET) $(TARGET)-dbg
