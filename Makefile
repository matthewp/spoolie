CC ?= cc
CFLAGS += -Wall -Wextra -g
LDFLAGS +=

# pkg-config for deps; fall back to direct flags for macOS
PKG_CFLAGS != pkg-config --cflags cups ncursesw 2>/dev/null || pkg-config --cflags cups ncurses 2>/dev/null || true
PKG_LDFLAGS != pkg-config --libs cups ncursesw 2>/dev/null || pkg-config --libs cups ncurses 2>/dev/null || echo "-lcups -lncurses"
CFLAGS += $(PKG_CFLAGS)
LDFLAGS += $(PKG_LDFLAGS) -lpthread

SRCS = src/main.c src/ui.c src/cups_api.c src/printers.c src/jobs.c
OBJS = $(SRCS:.c=.o)

all: spoolie

spoolie: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) spoolie

# Header dependencies
HDRS = src/ui.h src/cups_api.h src/printers.h src/jobs.h
src/main.o: src/main.c src/ui.h
src/ui.o: src/ui.c src/ui.h src/printers.h src/jobs.h src/cups_api.h
src/cups_api.o: src/cups_api.c src/cups_api.h
src/printers.o: src/printers.c src/printers.h src/cups_api.h
src/jobs.o: src/jobs.c src/jobs.h src/cups_api.h

.PHONY: all clean
