# rekta-core. Needs a C99 compiler and the system's windowing libraries (on Linux libwayland-client, libwayland-cursor,
# libxkbcommon and Xlib; Cocoa on macOS); nothing else. On Linux, WAYLAND=0 builds for X11 alone.
#   make            build/librekta.a and build/demo
#   make test       headless tests and the reference images (needs node for the image comparison)
#   make shots      the demo's script without a screen, as PNGs in build/shots
#   make sanitize   the tests and the demo script under AddressSanitizer and UBSan
#   make shared     the shared library: only the RK_API functions are seen from outside (make abi checks it)
#   make install    both libraries, the header, rekta.pc for pkg-config and the man page under PREFIX (/usr/local)
#   make gallery    the reference gallery's native half in build/gallery (node test/gallery.mjs holds the web to it)
#   make perf       the loop under load (resize, a storm of presses, settling), measured at 1x and 2x
# On Windows: build.bat (MSVC) or make with MinGW.

CC ?= cc
OPT ?= -O2
WARN := -std=c99 -Wall -Wextra -Wpedantic
# no fused multiply-add: the same pixels on every machine
CFLAGS += $(OPT) $(WARN) -ffp-contract=off
BUILD := build
UNAME := $(shell uname -s 2>/dev/null)
SOURCES := $(wildcard src/*.c src/*.h src/wayland/*) include/rekta.h third_party/stb_truetype.h third_party/stb_image.h
WAYLAND ?= 1
VERSION := $(shell awk '/^\#define RK_VERSION_(MAJOR|MINOR|PATCH) /{ v = v (v == "" ? "" : ".") $$3 } END { print v }' include/rekta.h)
MAJOR := $(word 1,$(subst ., ,$(VERSION)))
MINOR := $(word 2,$(subst ., ,$(VERSION)))
# before 1.0 a minor version may break the ABI, so it is part of the soname
SOVER := $(if $(filter 0,$(MAJOR)),0.$(MINOR),$(MAJOR))
PIC := -fPIC -fvisibility=hidden -DRK_SHARED -DRK_BUILD
PREFIX ?= /usr/local

ifeq ($(UNAME),Darwin)
  PLATFORM_OBJ := $(BUILD)/platform_cocoa.o
  LIBS := -framework Cocoa -framework QuartzCore
  SHLIB := librekta.$(SOVER).dylib
  SHLINKS := librekta.dylib
  SHFLAGS = -dynamiclib -install_name @rpath/$(SHLIB) -compatibility_version $(SOVER) -current_version $(VERSION)
else ifneq (,$(findstring MINGW,$(UNAME))$(findstring MSYS,$(UNAME))$(OS))
  LIBS := -luser32 -lgdi32 -ladvapi32
  EXE := .exe
  SHLIB := rekta.dll
  SHFLAGS = -shared -Wl,--out-implib,$(BUILD)/librekta.dll.a
  PIC := -DRK_SHARED -DRK_BUILD
else
  LIBS := -lX11 -lXext -lm -lpthread -ldl
  SHLIB := librekta.so.$(VERSION)
  SHLINKS := librekta.so.$(SOVER) librekta.so
  SHFLAGS = -shared -Wl,-soname,librekta.so.$(SOVER) -Wl,--no-undefined
  ifeq ($(WAYLAND),1)
    WAYLAND_PC := wayland-client wayland-cursor xkbcommon
    CFLAGS += $(shell pkg-config --cflags $(WAYLAND_PC) 2>/dev/null)
    PC_LIBS := -lrt $(LIBS)
    LIBS := $(or $(shell pkg-config --libs $(WAYLAND_PC) 2>/dev/null),-lwayland-client -lwayland-cursor -lxkbcommon) -lrt $(LIBS)
  else
    CFLAGS += -DRK_NO_WAYLAND
  endif
endif
# what rekta.pc names itself; the packages it requires bring the rest
PC_LIBS ?= $(LIBS)
HEADLESS_LIBS := -lm $(if $(EXE),,-lpthread)

all: $(BUILD)/librekta.a $(BUILD)/demo$(EXE) $(BUILD)/hello$(EXE) $(BUILD)/navigation$(EXE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/rekta.o: $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -c src/rekta.c -o $@

$(BUILD)/platform_cocoa.o: src/platform_cocoa.m src/rk_internal.h include/rekta.h | $(BUILD)
	$(CC) $(OPT) -fobjc-arc -Wall -c $< -o $@

$(BUILD)/librekta.a: $(BUILD)/rekta.o $(PLATFORM_OBJ)
	rm -f $@
	ar rcs $@ $^

$(BUILD)/rekta-pic.o: $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) $(PIC) -c src/rekta.c -o $@

$(BUILD)/platform_cocoa-pic.o: src/platform_cocoa.m src/rk_internal.h include/rekta.h | $(BUILD)
	$(CC) $(OPT) -fobjc-arc -Wall $(PIC) -c $< -o $@

$(BUILD)/$(SHLIB): $(BUILD)/rekta-pic.o $(PLATFORM_OBJ:.o=-pic.o)
	$(CC) $(SHFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	for l in $(SHLINKS); do ln -sf $(SHLIB) $(BUILD)/$$l; done

shared: $(BUILD)/$(SHLIB)

# the library's exports are exactly the header's RK_API functions, and a program links against it and runs
abi: $(BUILD)/$(SHLIB)
	sh test/abi.sh $(BUILD) $(SHLIB)

# written at each install, as it names PREFIX
$(BUILD)/rekta.pc: FORCE | $(BUILD)
	printf '%s\n' 'prefix=$(PREFIX)' 'libdir=$${prefix}/lib' 'includedir=$${prefix}/include' '' \
	  'Name: rekta' 'Description: Rekta Design for native desktop apps' 'Version: $(VERSION)' \
	  'Cflags: -I$${includedir}' 'Libs: -L$${libdir} -lrekta' 'Libs.private: $(strip $(PC_LIBS))' \
	  $(if $(WAYLAND_PC),'Requires.private: $(WAYLAND_PC)') > $@

$(BUILD)/demo$(EXE): demo/demo.c $(BUILD)/librekta.a
	$(CC) $(CFLAGS) $< $(BUILD)/librekta.a $(LIBS) -o $@

# examples ignore the arguments their callbacks do not need
$(BUILD)/hello$(EXE): examples/hello.c $(BUILD)/librekta.a
	$(CC) $(CFLAGS) -Wno-unused-parameter -Iinclude $< $(BUILD)/librekta.a $(LIBS) -o $@

$(BUILD)/navigation$(EXE): examples/navigation.c $(BUILD)/librekta.a
	$(CC) $(CFLAGS) -Iinclude $< $(BUILD)/librekta.a $(LIBS) -o $@

$(BUILD)/hello-headless$(EXE): examples/hello.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -Wno-unused-parameter -Iinclude -DRK_HEADLESS_ONLY $< src/rekta.c $(HEADLESS_LIBS) -o $@

$(BUILD)/test$(EXE): test/test.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -DRK_HEADLESS_ONLY test/test.c $(HEADLESS_LIBS) -o $@

$(BUILD)/demo-headless$(EXE): demo/demo.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -DRK_HEADLESS_ONLY demo/demo.c src/rekta.c $(HEADLESS_LIBS) -o $@

test: $(BUILD)/test$(EXE) $(BUILD)/hello-headless$(EXE)
	$(BUILD)/test$(EXE) $(BUILD)/test-out
	node test/compare.mjs test/ref $(BUILD)/test-out
	$(BUILD)/hello-headless$(EXE)
	sh test/examples.sh

$(BUILD)/gallery-bin$(EXE): test/gallery.c $(SOURCES) | $(BUILD)
	$(CC) $(CFLAGS) -DRK_HEADLESS_ONLY -Iinclude test/gallery.c src/rekta.c $(HEADLESS_LIBS) -o $@

gallery: $(BUILD)/gallery-bin$(EXE)
	mkdir -p $(BUILD)/gallery
	$(BUILD)/gallery-bin$(EXE) $(BUILD)/gallery

shots: $(BUILD)/demo-headless$(EXE)
	mkdir -p $(BUILD)/shots
	$(BUILD)/demo-headless$(EXE) --shots $(BUILD)/shots

perf: $(BUILD)/demo-headless$(EXE)
	$(BUILD)/demo-headless$(EXE) --perf
	RK_SCALE=2 $(BUILD)/demo-headless$(EXE) --perf

sanitize: | $(BUILD)
	$(CC) -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -DRK_HEADLESS_ONLY test/test.c -lm -lpthread -o $(BUILD)/test-asan
	$(CC) -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -DRK_HEADLESS_ONLY demo/demo.c src/rekta.c -lm -lpthread -o $(BUILD)/demo-asan
	$(BUILD)/test-asan $(BUILD)/test-asan-out
	mkdir -p $(BUILD)/shots-asan
	$(BUILD)/demo-asan --shots $(BUILD)/shots-asan > /dev/null

install: $(BUILD)/librekta.a $(BUILD)/$(SHLIB) $(BUILD)/rekta.pc
	mkdir -p $(DESTDIR)$(PREFIX)/lib/pkgconfig $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/share/man/man3
	cp $(BUILD)/librekta.a $(DESTDIR)$(PREFIX)/lib/
	cp $(BUILD)/$(SHLIB) $(DESTDIR)$(PREFIX)/lib/
	for l in $(SHLINKS); do ln -sf $(SHLIB) $(DESTDIR)$(PREFIX)/lib/$$l; done
ifeq ($(UNAME),Darwin)
	# installed, it is found where it lies; changing the name means signing again
	install_name_tool -id $(PREFIX)/lib/$(SHLIB) $(DESTDIR)$(PREFIX)/lib/$(SHLIB)
	codesign -f -s - $(DESTDIR)$(PREFIX)/lib/$(SHLIB)
endif
	cp $(BUILD)/rekta.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/
	cp include/rekta.h $(DESTDIR)$(PREFIX)/include/
	cp man/rekta.3 $(DESTDIR)$(PREFIX)/share/man/man3/

clean:
	rm -rf $(BUILD)

.PHONY: all test gallery shots perf sanitize shared abi install clean FORCE
