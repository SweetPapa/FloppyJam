# THE GAUNTLET — single binary, no shipped assets (§11.1).
#   make            release build (-Os -flto, stripped)
#   make TUNING=1   tuning build with the debug keys from §4.4
#   make run

CC      ?= cc
TARGET  := gauntlet
SRCDIR  := src

SRCS := $(SRCDIR)/main.c \
        $(SRCDIR)/game.c \
        $(SRCDIR)/screens.c \
        $(SRCDIR)/audio.c \
        $(SRCDIR)/sequencer.c \
        $(SRCDIR)/voices.c \
        $(SRCDIR)/chart.c \
        $(SRCDIR)/clock.c \
        $(SRCDIR)/cards.c \
        $(SRCDIR)/heat.c \
        $(SRCDIR)/draw.c \
        $(SRCDIR)/persist.c \
        $(SRCDIR)/rng.c \
        $(SRCDIR)/micro/micro_common.c \
        $(SRCDIR)/micro/micro_a.c \
        $(SRCDIR)/micro/micro_b.c \
        $(SRCDIR)/micro/micro_c.c \
        $(SRCDIR)/micro/roster.c

OBJS := $(SRCS:.c=.o)

CFLAGS  := -std=c11 -Os -flto -Wall -Wextra -Wno-unused-parameter \
           -I$(SRCDIR) -I$(SRCDIR)/micro
LDFLAGS := -flto
LDLIBS  := -lm

ifdef TUNING
CFLAGS += -DTUNING -g -O1
endif

UNAME_S := $(shell uname -s)

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
RAYLIB_LIBS := -lraylib
endif
CFLAGS += $(RAYLIB_CFLAGS)
LDLIBS += $(RAYLIB_LIBS)

ifeq ($(UNAME_S),Darwin)
LDLIBS += -framework CoreVideo -framework IOKit -framework Cocoa \
          -framework GLUT -framework OpenGL
endif
ifeq ($(UNAME_S),Linux)
LDLIBS += -lpthread -ldl -lrt -lX11
endif

.PHONY: all clean run tuning

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
ifndef TUNING
	-strip $@
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) --windowed

clean:
	rm -f $(OBJS) $(TARGET)
