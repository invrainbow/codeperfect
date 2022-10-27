CC = clang++

CFLAGS = -std=c++17 -I. -Iimgui/ -ferror-limit=100 -Itree-sitter/lib/include
CFLAGS += -Wno-switch -Wno-writable-strings -Wno-arc-performSelector-leaks -Wno-deprecated
# CFLAGS += -mavx -maes

ifeq ($(TESTING_BUILD), 1)
	CFLAGS += -DTESTING_BUILD
endif

LDFLAGS =
LDFLAGS += obj/gohelper.a
PKGS =

BINARY_SUFFIX =

ifeq ($(OSTYPE), mac)
	CFLAGS += -DOSTYPE_MAC
	PKGS += fontconfig freetype2 libpcre harfbuzz

	frameworks = OpenGL Cocoa IOKit CoreFoundation Security
	LDFLAGS += $(foreach it, $(frameworks), -framework $(it))

	ifeq ($(shell sh/detect_m1), 1)
		CFLAGS += -arch arm64
		GOARCH = arm64
	else
		CFLAGS += -arch x86_64
		GOARCH = amd64
	endif
else ifeq ($(OSTYPE), windows)
	# a bunch of clang on windows specific warnings idgaf about
	CFLAGS += -Wno-ignored-attributes -Wno-constant-conversion -Wno-static-in-inline -Wno-microsoft-include
	BINARY_SUFFIX = .exe
	GOARCH = amd64
	CFLAGS += -DOSTYPE_WINDOWS
	PKGS += fontconfig freetype2 libpcre glfw3 harfbuzz
	LDFLAGS += -lopengl32 -ladvapi32 -lshlwapi -lole32 -lpathcch -lshell32 -lwinmm -lws2_32 -lgdi32 -lshcore  # link windows dlls
	LDFLAGS += --for-linker "/IGNORE:4217"  # do we still need this?
else ifeq ($(OSTYPE), linux)
	CFLAGS += -DOSTYPE_LINUX -arch x86_64
	GOARCH = amd64
	PKGS += fontconfig freetype2 libpcre gtk+-3.0 glfw3 gl harfbuzz
endif

CFLAGS += $(shell sh/pkgconfig --cflags $(PKGS))
LDFLAGS += $(shell sh/pkgconfig --libs $(PKGS))

GOLDFLAGS =
ifeq ($(RELEASE), 1)
	# CFLAGS += -O3
	CFLAGS += -g -O3
	GOLDFLAGS += -s -w
	# LDFLAGS += -Wl,-S,-x
else
	CFLAGS += -DDEBUG_BUILD -g -O0
	CFLAGS += -MMD -MP
endif

PYTHON = python3

SEPARATE_SRC_FILES = tests.cpp enums.cpp
SRC_FILES := $(filter-out $(SEPARATE_SRC_FILES), $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))
DEP_FILES += obj/clibs.d
OBJ_DEPS = $(OBJ_FILES) obj/clibs.o obj/gohelper.a
GO_DEPS = $(shell find go/ -type f -name '*.go')

ifeq ($(OSTYPE), mac)
	DEP_FILES += obj/objclibs.d
	OBJ_DEPS += obj/objclibs.o
endif

.PHONY: all test clean prep launcher

all: build/bin/ide$(BINARY_SUFFIX) build/bin/init.vim build/bin/buildcontext.go

launcher: build/launcher$(BINARY_SUFFIX)

test: build/bin/test build/bin/init.vim build/bin/buildcontext.go

prep:
	mkdir -p obj build/bin

clean:
	rm -rf obj/ build/bin/
	mkdir -p obj build/bin

build/bin/test: $(filter-out obj/main.o, $(OBJ_DEPS)) obj/tests.o obj/enums.o binaries.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDFLAGS)

build/bin/ide$(BINARY_SUFFIX): $(OBJ_DEPS) binaries.c obj/enums.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: %.cpp gohelper.h tstypes.hpp enums.hpp # Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

obj/enums.o: enums.cpp # Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

obj/tests.o: tests.cpp # Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

obj/objclibs.o: objclibs.mm
	$(CC) $(CFLAGS) -fobjc-arc -c -o $@ $<

PIC_FLAGS =
ifeq ($(OSTYPE), windows)
else
	PIC_FLAGS = -fPIC
endif

obj/clibs.o: clibs.c
	clang $(CFLAGS) -std=gnu99 $(PIC_FLAGS) -c -o $@ $<

binaries.c: .cpcolors vert.glsl frag.glsl im.vert.glsl im.frag.glsl
	$(PYTHON) sh/create_binaries_c.py $^

obj/gohelper.a: $(GO_DEPS)
	cd go; \
		GOARCH=$(GOARCH) CC=clang CGO_ENABLED=1 go build -ldflags "$(GOLDFLAGS)" -o gohelper.a -buildmode=c-archive ./helper && \
		(mkdir -p ../obj; mv gohelper.a ../obj; mv gohelper.h ..)

LAUNCHER_LDFLAGS =
ifeq ($(OSTYPE), windows)
	LAUNCHER_LDFLAGS += -H windowsgui
endif

build/launcher$(BINARY_SUFFIX): $(GO_DEPS)
	cd go; \
		GOARCH=$(GOARCH) go build -ldflags "$(GOLDFLAGS) $(LAUNCHER_LDFLAGS)" -o ../$@ ./cmd/launcher

gohelper.h: obj/gohelper.a

tstypes.hpp: tree-sitter-go/src/parser.c sh/generate_tstypes.py
	$(PYTHON) sh/generate_tstypes.py

enums.cpp: enums.hpp

enums.hpp: $(filter-out enums.hpp, $(wildcard *.hpp)) tstypes.hpp sh/generate_enums.py
	$(PYTHON) sh/generate_enums.py

build/bin/buildcontext.go: go/buildcontext/main.go
	cp go/buildcontext/main.go build/bin/buildcontext.go

build/bin/init.vim: init.vim
	cp init.vim build/bin/init.vim
