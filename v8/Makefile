CC ?= cc
TARGET := pulsewing
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
PURE := src/sim.c src/spawn.c src/routes.c src/ghost.c
WARN := -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS := -std=c11 -Os -flto -ffunction-sections -fdata-sections $(WARN) -Isrc $(shell pkg-config --cflags raylib 2>/dev/null)
LDLIBS := -lm $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(shell pkg-config --libs raylib 2>/dev/null)),)
LDLIBS += -lraylib
endif
ifeq ($(shell uname -s),Darwin)
LDLIBS += -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL
endif
ifdef DEBUG
CFLAGS := -std=c11 -O1 -g $(WARN) -Isrc $(shell pkg-config --cflags raylib 2>/dev/null)
endif
.PHONY: all debug run graybox test probes check web clean
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) -flto -o $@ $^ $(LDLIBS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
debug:
	$(MAKE) DEBUG=1
run: $(TARGET)
	./$(TARGET)
graybox: $(TARGET)
	./$(TARGET) --graybox
test:
	$(CC) -std=c11 -O2 $(WARN) -Isrc tests/test_sim.c $(PURE) -lm -o pw_test_sim
	./pw_test_sim
probes:
	$(CC) -std=c11 -O2 $(WARN) -Isrc tests/test_probes.c src/routes.c -lm -o pw_test_probes
	./pw_test_probes
check: test probes $(TARGET)
	@echo "PULSEWING check: green"
RAYLIB_WEB ?= $(HOME)/raylib-web
web:
	@command -v emcc >/dev/null || { echo "emcc not found"; exit 1; }
	@test -f $(RAYLIB_WEB)/lib/libraylib.a || { echo "set RAYLIB_WEB to an emscripten raylib"; exit 1; }
	@mkdir -p build/web
	emcc -o build/web/index.html $(SRC) -std=c11 -Os -Isrc -I$(RAYLIB_WEB)/include \
	  $(RAYLIB_WEB)/lib/libraylib.a -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 \
	  -s FORCE_FILESYSTEM=0 -DPLATFORM_WEB --shell-file web/shell.html
clean:
	rm -f $(OBJ) $(TARGET) pw_test_sim pw_test_probes
	rm -rf build
