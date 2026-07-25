# BREAK PAR — single binary, no shipped assets (Section 2, 13).
#
#   make               release build (-Os -flto, stripped)
#   make debug         warnings + symbols, no LTO
#   make run           build and play
#   make test          headless physics suite (Definition of Done 17.2)
#   make testcourse    geometry + playthrough bot over all 18 holes
#   make size          total fileset bytes; fails over the 1.44 MB ceiling
#   make windows       cross-compile Windows x64 via MinGW-w64
#   make clean

CC       ?= cc
TARGET   := breakpar
SRCDIR   := src

SRCS := $(SRCDIR)/main.c \
        $(SRCDIR)/physics.c \
        $(SRCDIR)/course.c \
        $(SRCDIR)/shot.c \
        $(SRCDIR)/camera.c \
        $(SRCDIR)/render.c \
        $(SRCDIR)/scenery.c \
        $(SRCDIR)/juice.c \
        $(SRCDIR)/synth.c \
        $(SRCDIR)/save.c \
        $(SRCDIR)/replay.c

OBJS := $(SRCS:.c=.o)

WARN     := -Wall -Wextra -Wno-unused-parameter
CFLAGS   := -std=c11 -Os -flto -ffunction-sections -fdata-sections $(WARN) -I$(SRCDIR)
LDFLAGS  := -flto
LDLIBS   := -lm

ifdef DEBUG
CFLAGS   := -std=c11 -O1 -g $(WARN) -I$(SRCDIR)
LDFLAGS  :=
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

# 1,474,560 bytes: one 1.44 MB floppy. Hard ceiling, not a target.
FLOPPY := 1474560

.PHONY: all debug run test testcourse testcamera testmusic size windows clean

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

# --- tests: physics.c and course.c build standalone, no window needed ---

test:
	$(CC) -std=c11 -O2 $(WARN) -I$(SRCDIR) tests/test_physics.c $(SRCDIR)/physics.c \
	      -lm -o bp_test_physics
	./bp_test_physics

testcourse:
	$(CC) -std=c11 -O2 $(WARN) -I$(SRCDIR) -I. tests/test_course.c \
	      $(SRCDIR)/course.c $(SRCDIR)/physics.c $(SRCDIR)/shot.c \
	      -lm -o bp_test_course
	./bp_test_course

# The combo, rendered offline against stub audio calls: no window, no device.
# Writes build/music-*.wav so the music can actually be listened to on a build
# machine, and asserts the failure modes a generative bed is prone to.
testmusic:
	@mkdir -p build
	$(CC) -std=c11 -O2 $(WARN) -I$(SRCDIR) -I. $(RAYLIB_CFLAGS) \
	      tests/test_music.c $(SRCDIR)/synth.c -lm -o bp_test_music
	./bp_test_music

testcamera:
	$(CC) -std=c11 -O2 $(WARN) -I$(SRCDIR) -I. $(RAYLIB_CFLAGS) tests/test_camera.c \
	      $(SRCDIR)/camera.c $(SRCDIR)/course.c $(SRCDIR)/physics.c \
	      $(LDLIBS) -o bp_test_camera
	./bp_test_camera

size: $(TARGET)
	@total=$$(wc -c < $(TARGET)); \
	printf 'fileset: %s bytes (%s KB) of %s\n' "$$total" "$$((total/1024))" "$(FLOPPY)"; \
	if [ "$$total" -gt "$(FLOPPY)" ]; then \
	  echo "OVER THE FLOPPY CEILING"; exit 1; \
	else \
	  printf 'headroom: %s KB\n' "$$(( ($(FLOPPY)-total)/1024 ))"; \
	fi

# Windows x64 is the primary contest target; needs a MinGW raylib in
# RAYLIB_WIN (headers in $(RAYLIB_WIN)/include, libraylib.a in .../lib).
RAYLIB_WIN ?= /usr/local/x86_64-w64-mingw32
# The icon and version block are compiled by windres and linked in, so the
# executable carries its own artwork instead of needing a file next to it. If
# windres or the .ico is missing the build still succeeds, just iconless — an
# icon should never be the reason a release cannot be produced.
windows:
	@if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1 && [ -f ../art/breakpar.ico ]; then \
	  echo "x86_64-w64-mingw32-windres $(SRCDIR)/win.rc -O coff -o win.res.o"; \
	  x86_64-w64-mingw32-windres $(SRCDIR)/win.rc -O coff -o win.res.o; \
	else \
	  echo "note: no windres or no art/breakpar.ico — building without an icon"; \
	  rm -f win.res.o; \
	fi
	x86_64-w64-mingw32-gcc -std=c11 -Os -flto -ffunction-sections -fdata-sections \
	  $(WARN) -I$(SRCDIR) -I$(RAYLIB_WIN)/include $(SRCS) \
	  $$( [ -f win.res.o ] && echo win.res.o ) \
	  -L$(RAYLIB_WIN)/lib -lraylib -lopengl32 -lgdi32 -lwinmm \
	  -static -static-libgcc -mwindows \
	  -Wl,--gc-sections -o $(TARGET).exe
	x86_64-w64-mingw32-strip $(TARGET).exe
	@rm -f win.res.o
	@total=$$(wc -c < $(TARGET).exe); \
	 printf '%s: %s bytes (%s KB) of %s\n' "$(TARGET).exe" "$$total" "$$((total/1024))" "$(FLOPPY)"; \
	 [ "$$total" -le "$(FLOPPY)" ] || { echo "OVER THE FLOPPY CEILING"; exit 1; }

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe win.res.o \
	      bp_test_physics bp_test_course bp_test_camera bp_test_music
