# HUEDUNIT — one binary, zero asset files (Definition of Done §18.5).
#
#   make            release build (-O2 -flto, stripped)
#   make debug      -O1 -g, no LTO
#   make run        build and play
#   make bake       compile content/ only (bakery + linters, no game build)
#   make check      build + unit tests + content lint + puzzle solve fixtures
#   make size       shipped bytes; reports executable size (no asset budget)
#   make windows    cross-compile Windows x64 via MinGW-w64
#   make clean

CC     ?= cc
TARGET := huedunit
SRCDIR := src

# Auto-discovery, no registries (API.md §7.3): the build globs modules and
# puzzles. Adding src/puzzles/p_thing.c is the entire act of adding a puzzle.
SRCS  := $(wildcard $(SRCDIR)/*/*.c)
OBJS  := $(SRCS:.c=.o)

CONTENT  := $(wildcard content/*/*)
BAKED    := $(SRCDIR)/generated/content_data.h
BAKERY   := tools/bakery/bakery

WARN   := -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS := -std=c11 -O2 -flto -ffunction-sections -fdata-sections $(WARN) -I$(SRCDIR)
LDFLAGS := -flto
LDLIBS  := -lm

ifdef DEBUG
CFLAGS  := -std=c11 -O1 -g $(WARN) -I$(SRCDIR)
LDFLAGS :=
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
LDLIBS  += -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL
endif
ifeq ($(UNAME_S),Linux)
LDFLAGS += -Wl,--gc-sections
LDLIBS  += -lpthread -ldl -lrt -lX11
endif

# Size is informational; favor readability, assets and playability.


.PHONY: all debug run bake check test size windows clean native-smoke

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
ifndef DEBUG
	-strip $@
endif

$(OBJS): $(BAKED) $(SRCDIR)/generated/font_data.h

$(SRCDIR)/generated/font_data.h: assets/fonts/Barlow-Medium.ttf tools/embed_font.py
	python3 tools/embed_font.py

-include $(OBJS:.o=.d)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# --- content pipeline ------------------------------------------------------
# bakery parses all four grammars, runs linters 3-6 from §10, and emits the
# blob. A content error fails the build, which is the whole point.
$(BAKERY): tools/bakery/bakery.c
	$(CC) -std=c11 -O2 -Wall -Wextra -o $@ $<

$(BAKED): $(BAKERY) $(CONTENT)
	@mkdir -p $(SRCDIR)/generated
	./$(BAKERY) content $(BAKED)

bake: $(BAKED)

debug:
	$(MAKE) clean
	$(MAKE) DEBUG=1

run: $(TARGET)
	./$(TARGET)

# --- tests -----------------------------------------------------------------
# Every test links the whole engine except main.c and runs with no window and
# no audio device: nothing under test calls into the GPU or the mixer.
TESTCFLAGS := -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -I$(SRCDIR) $(RAYLIB_CFLAGS)
ENGINE     := $(filter-out $(SRCDIR)/core/main.c,$(SRCS))

test: $(BAKED) $(SRCDIR)/generated/font_data.h
	$(CC) $(TESTCFLAGS) tests/test_flags.c $(SRCDIR)/flags/flags.c -lm -o hd_test_flags
	python3 tests/run_isolated.py ./hd_test_flags
	$(CC) $(TESTCFLAGS) tests/test_content.c $(ENGINE) $(LDLIBS) -o hd_test_content
	python3 tests/run_isolated.py ./hd_test_content
	$(CC) $(TESTCFLAGS) tests/test_puzzles.c $(ENGINE) $(LDLIBS) -o hd_test_puzzles
	python3 tests/run_isolated.py ./hd_test_puzzles

	$(CC) $(TESTCFLAGS) tests/test_campaign.c $(ENGINE) $(LDLIBS) -o hd_test_campaign
	python3 tests/run_isolated.py ./hd_test_campaign

check: bake test $(TARGET) size
	@echo "check: green"

size: $(TARGET)
	@wc -c $(TARGET)

# Windows x64 is the primary ship target (§1.2.6); needs a MinGW raylib in
# RAYLIB_WIN (headers in $(RAYLIB_WIN)/include, libraylib.a in .../lib).
RAYLIB_WIN ?= /usr/local/x86_64-w64-mingw32
windows: $(BAKED) $(SRCDIR)/generated/font_data.h
	x86_64-w64-mingw32-gcc -std=c11 -O2 -flto -ffunction-sections -fdata-sections \
	  $(WARN) -I$(SRCDIR) -I$(RAYLIB_WIN)/include $(SRCS) \
	  -L$(RAYLIB_WIN)/lib -lraylib -lopengl32 -lgdi32 -lwinmm \
	  -static -static-libgcc -mwindows \
	  -Wl,--gc-sections -o $(TARGET).exe
	x86_64-w64-mingw32-strip $(TARGET).exe

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET) $(TARGET).exe $(BAKERY) $(BAKED) \
	      hd_test_flags hd_test_content hd_test_puzzles hd_test_campaign huedunit_smoke

# Real window + production app, deterministic mouse/keyboard polling.
# Run from an isolated capture folder: huedunit_smoke never writes player saves.
native-smoke: $(BAKED) $(SRCDIR)/generated/font_data.h
	$(CC) $(TESTCFLAGS) tests/native_smoke.c $(ENGINE) $(LDLIBS) -o huedunit_smoke
