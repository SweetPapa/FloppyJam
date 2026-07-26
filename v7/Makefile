# VOLLEYBAR — one binary, zero asset files (§0).
#
#   make            release build (-Os -flto, stripped)
#   make debug      -O1 -g, no LTO
#   make run        build and play
#   make graybox    the pre-beauty build: rectangles, no shaders (§14.1, DoD 6)
#   make test       headless suites: determinism, tunneling, strikes, rules
#   make contrast   the readability probe (Pillar 4) — offscreen, no window
#   make check      build + test + contrast + size. Merge-blocking.
#   make size       shipped bytes against the 1.44 MB floppy ceiling
#   make web        emscripten build -> build/web/index.html|.js|.wasm
#   make windows    cross-compile Windows x64 via MinGW-w64
#   make clean

CC     ?= cc
TARGET := volleybar
SRCDIR := src

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(SRCS:.c=.o)

# The pure half: no raylib, no window, no audio device, no wall clock. Every
# headless suite links exactly this set, which is also the §13 lane contract.
SIMSRC := $(SRCDIR)/sim.c $(SRCDIR)/rods.c $(SRCDIR)/heat.c \
          $(SRCDIR)/rules.c $(SRCDIR)/ai.c $(SRCDIR)/replay.c

WARN    := -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS  := -std=c11 -Os -flto -ffunction-sections -fdata-sections $(WARN) -I$(SRCDIR)
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

# 1,474,560 bytes: one 1.44 MB floppy.
#
# [W0] §0 of the spec says no size ceiling is imposed on this project. It ships
# under one anyway, exactly as v6 decided: the FloppyJam rule is the repo's
# whole identity and the procedural mandate makes it free. Quality still wins
# every conflict — nothing here is cut for bytes, it simply never cost any.
FLOPPY := 1474560

.PHONY: all debug run graybox test contrast check size web windows clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
ifndef DEBUG
	-strip $@
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) DEBUG=1

run: $(TARGET)
	./$(TARGET)

# DoD 6: the gray-box build is retained forever as proof that the fun predates
# the glow. It is the same binary — the flag just refuses to load the shaders.
graybox: $(TARGET)
	./$(TARGET) --graybox

# --- tests -----------------------------------------------------------------
TESTCFLAGS := -std=c11 -O2 -g $(WARN) -I$(SRCDIR)

test:
	$(CC) $(TESTCFLAGS) tests/test_sim.c $(SIMSRC) -lm -o vb_test_sim
	./vb_test_sim
	$(CC) $(TESTCFLAGS) tests/test_rules.c $(SIMSRC) -lm -o vb_test_rules
	./vb_test_rules

# Pillar 4 has a law, so it gets a probe rather than an opinion: worst-case
# frames are rendered to an offscreen target and the ball's luminance delta
# against everything under it is measured. A shader that hides the ball fails
# the build.
contrast:
	$(CC) $(TESTCFLAGS) tests/test_contrast.c \
	      $(SRCDIR)/palette.c $(SRCDIR)/heat.c -lm -o vb_test_contrast
	./vb_test_contrast

check: test contrast $(TARGET) size
	@echo "check: green"

size: $(TARGET)
	@total=$$(wc -c < $(TARGET)); \
	printf 'shipped: %s bytes (%s KB) of %s\n' "$$total" "$$((total/1024))" "$(FLOPPY)"; \
	if [ "$$total" -gt "$(FLOPPY)" ]; then \
	  echo "OVER THE FLOPPY CEILING"; exit 1; \
	else \
	  printf 'headroom: %s KB\n' "$$(( ($(FLOPPY)-total)/1024 ))"; \
	fi

# --- web: first-class, not an afterthought (§0) ----------------------------
# One app_frame() entry from day one, single-threaded sim, audio started on the
# first input gesture. Needs a raylib built with emcc in RAYLIB_WEB.
RAYLIB_WEB ?= $(HOME)/raylib-web
web:
	@command -v emcc >/dev/null 2>&1 || { \
	  echo "emcc not found — source emsdk_env.sh first"; exit 1; }
	@test -f $(RAYLIB_WEB)/lib/libraylib.a || { \
	  echo "no web raylib at $(RAYLIB_WEB)/lib/libraylib.a"; \
	  echo "build one: cd raylib/src && make PLATFORM=PLATFORM_WEB"; exit 1; }
	@mkdir -p build/web
	emcc -o build/web/index.html $(SRCS) \
	  -std=c11 -Os -I$(SRCDIR) -I$(RAYLIB_WEB)/include \
	  -Wall -Wextra -Wno-unused-parameter \
	  $(RAYLIB_WEB)/lib/libraylib.a \
	  -s USE_GLFW=3 -s ASYNCIFY=0 -s ALLOW_MEMORY_GROWTH=1 \
	  -s FORCE_FILESYSTEM=0 -s TOTAL_STACK=1048576 \
	  -DPLATFORM_WEB --shell-file web/shell.html
	@printf 'web build: '; wc -c < build/web/index.wasm

# --- windows ---------------------------------------------------------------
RAYLIB_WIN ?= /usr/local/x86_64-w64-mingw32
windows:
	x86_64-w64-mingw32-gcc -std=c11 -Os -flto -ffunction-sections -fdata-sections \
	  $(WARN) -I$(SRCDIR) -I$(RAYLIB_WIN)/include $(SRCS) \
	  -L$(RAYLIB_WIN)/lib -lraylib -lopengl32 -lgdi32 -lwinmm \
	  -static -static-libgcc -mwindows \
	  -Wl,--gc-sections -o $(TARGET).exe
	x86_64-w64-mingw32-strip $(TARGET).exe
	@total=$$(wc -c < $(TARGET).exe); \
	 printf '%s: %s bytes (%s KB) of %s\n' "$(TARGET).exe" "$$total" "$$((total/1024))" "$(FLOPPY)"; \
	 [ "$$total" -le "$(FLOPPY)" ] || { echo "OVER THE FLOPPY CEILING"; exit 1; }

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe \
	      vb_test_sim vb_test_rules vb_test_contrast
	rm -rf build/web
