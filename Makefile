PREFIX ?= /usr/local
OPTLEVEL ?= -O3 -march=native -flto

.PHONY: all clean install
.DEFAULT: all

LIBS += jansson libcurl
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -pipe
CFLAGS += $(shell pkg-config  $(LIBS) --cflags)
LDFLAGS ?= -Wl,-O2 -Wl,--as-needed -Wl,-flto
LDFLAGS += $(shell pkg-config $(LIBS) --libs)

PROG = poolf
DIR_BUILD = build
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(DIR_BUILD)/%.o,$(SRCS))
DEPS = $(patsubst %.c,$(DIR_BUILD)/%.d,$(SRCS))

$(info $(shell mkdir -p $(DIR_BUILD)))

$(DIR_BUILD)/%.o: %.c
	$(CC) $(OPTLEVEL) $(CFLAGS) -o $@ -c -MMD $<

$(PROG): $(OBJS)
	$(CC) $(OPTLEVEL) $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: $(PROG)

clean:
	$(RM) $(PROG) $(OBJS) $(DEPS)

DESTINATION = $(DESTDIR)$(PREFIX)
install: all
	install -m 0755 $(PROG) $(DESTINATION)/bin

-include $(DEPS)
