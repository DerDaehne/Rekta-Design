# rekta-core. Needs a C99 compiler and the system's windowing library (X11 on Linux, Cocoa on macOS); nothing else.
#   make            build/librekta.a and build/demo
#   make test       headless tests and the reference images (needs node for the image comparison)
#   make shots      the demo's script without a screen, as PNGs in build/shots
#   make sanitize   the tests and the demo script under AddressSanitizer and UBSan
# On Windows: build.bat (MSVC) or make with MinGW.

CC ?= cc
OPT ?= -O2
WARN := -std=c99 -Wall -Wextra -Wpedantic
# no fused multiply-add: the same pixels on every machine
CFLAGS += $(OPT) $(WARN) -ffp-contract=off
BUILD := build
UNAME := $(shell uname -s 2>/dev/null)
SOURCES := $(wildcard src/*.c src/*.h) include/rekta.h third_party/stb_truetype.h

ifeq ($(UNAME),Darwin)
  PLATFORM_OBJ := $(BUILD)/platform_cocoa.o
  LIBS := -framework Cocoa -framework QuartzCore
else ifneq (,$(findstring MINGW,$(UNAME))$(findstring MSYS,$(UNAME))$(OS))
  LIBS := -luser32 -lgdi32 -ladvapi32
  EXE := .exe
else
  LIBS := -lX11 -lm -lpthread
endif
HEADLESS_LIBS := -lm $(if $(EXE),,-lpthread)

all: $(BUILD)/librekta.a $(BUILD)/demo$(EXE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/rekta.o: $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -c src/rekta.c -o $@

$(BUILD)/platform_cocoa.o: src/platform_cocoa.m src/rk_internal.h include/rekta.h | $(BUILD)
	$(CC) $(OPT) -fobjc-arc -Wall -c $< -o $@

$(BUILD)/librekta.a: $(BUILD)/rekta.o $(PLATFORM_OBJ)
	rm -f $@
	ar rcs $@ $^

$(BUILD)/demo$(EXE): demo/demo.c $(BUILD)/librekta.a
	$(CC) $(CFLAGS) $< $(BUILD)/librekta.a $(LIBS) -o $@

$(BUILD)/test$(EXE): test/test.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -DRK_HEADLESS_ONLY test/test.c $(HEADLESS_LIBS) -o $@

$(BUILD)/demo-headless$(EXE): demo/demo.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -DRK_HEADLESS_ONLY demo/demo.c src/rekta.c $(HEADLESS_LIBS) -o $@

test: $(BUILD)/test$(EXE)
	$(BUILD)/test$(EXE) $(BUILD)/test-out
	node test/compare.mjs test/ref $(BUILD)/test-out

shots: $(BUILD)/demo-headless$(EXE)
	mkdir -p $(BUILD)/shots
	$(BUILD)/demo-headless$(EXE) --shots $(BUILD)/shots

sanitize: | $(BUILD)
	$(CC) -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -DRK_HEADLESS_ONLY test/test.c -lm -lpthread -o $(BUILD)/test-asan
	$(CC) -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -DRK_HEADLESS_ONLY demo/demo.c src/rekta.c -lm -lpthread -o $(BUILD)/demo-asan
	$(BUILD)/test-asan $(BUILD)/test-asan-out
	mkdir -p $(BUILD)/shots-asan
	$(BUILD)/demo-asan --shots $(BUILD)/shots-asan > /dev/null

clean:
	rm -rf $(BUILD)

.PHONY: all test shots sanitize clean
