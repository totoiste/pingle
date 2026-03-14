# Makefile — pingle
# Conforms to GNU Coding Standards.
# Usage:
#   make              build optimised binary
#   make debug        build with debug symbols and sanitizers
#   make install      install to $(PREFIX)/bin
#   make uninstall    remove installed binary
#   make clean        remove build artefacts

# ── Toolchain ────────────────────────────────────────────────────────────────
CC      = gcc
INSTALL = install

# ── Directories ──────────────────────────────────────────────────────────────
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

# ── Source ───────────────────────────────────────────────────────────────────
TARGET  = pingle
SRC     = pingle.c

# ── Flags ────────────────────────────────────────────────────────────────────

# Release: optimise for size (-Os) and strip dead code sections.
CFLAGS  = -Wall -Wextra -Wpedantic \
          -Wformat-security -Wshadow \
          -Wstrict-prototypes -Wconversion -Wsign-conversion \
          -O2 -Os \
          -ffunction-sections -fdata-sections

LDFLAGS = -Wl,--gc-sections

# Libraries must appear AFTER object files on the linker command line
# so that ld can resolve symbols forward-referenced by the object code.
# -lm is needed for sqrt() used in RTT mdev computation.
LDLIBS  = -lm

# Debug: no optimisation, full sanitizers, debug symbols.
CFLAGS_DEBUG = -Wall -Wextra -Wpedantic \
               -Wformat-security -Wshadow \
               -Wstrict-prototypes -Wconversion -Wsign-conversion \
               -g -O0 \
               -fsanitize=address,undefined,signed-integer-overflow \
               -fno-omit-frame-pointer \
               -fanalyzer

# ── Rules ─────────────────────────────────────────────────────────────────────

.PHONY: all debug strip install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
	@echo "Built $@ (release)"

debug: $(SRC)
	$(CC) $(CFLAGS_DEBUG) -o $(TARGET)-debug $< $(LDLIBS)
	@echo "Built $(TARGET)-debug (debug + sanitizers)"

strip: $(TARGET)
	strip --strip-all $(TARGET)
	@echo "Stripped $(TARGET) (symbols removed)"
	size $(TARGET)

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	strip --strip-all $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Installed $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Removed $(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f $(TARGET) $(TARGET)-debug
	@echo "Cleaned"
