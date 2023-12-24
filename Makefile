CC = clang++

CFLAGS = -std=c++17 -I. -Isrc -Isrc/imgui/ -ferror-limit=100 -Isrc/tree-sitter/lib/include
CFLAGS += -Wno-switch -Wno-writable-strings -Wno-arc-performSelector-leaks -Wno-deprecated

ifeq ($(TESTING_BUILD), 1)
	CFLAGS += -DTESTING_BUILD
endif

LDFLAGS =
LDFLAGS += obj/gohelper.a
PKGS =

BINARY_SUFFIX =

CFLAGS += -mmacosx-version-min=10.12
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

CFLAGS += $(shell sh/pkgconfig --cflags $(PKGS))
LDFLAGS += $(shell sh/pkgconfig --libs $(PKGS))

GOLDFLAGS =
ifeq ($(RELEASE), 1)
	CFLAGS += -g -O3
	GOLDFLAGS += -s -w
else
	CFLAGS += -DDEBUG_BUILD -g -O0
	CFLAGS += -MMD -MP
endif

PYTHON = python3

SEPARATE_SRC_FILES = src/enums.cpp
SRC_FILES := $(filter-out $(SEPARATE_SRC_FILES), $(wildcard src/*.cpp))
OBJ_FILES = $(patsubst src/%.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst src/%.cpp,obj/%.d,$(SRC_FILES))
DEP_FILES += obj/clibs.d obj/tsgo.d
OBJ_DEPS = $(OBJ_FILES) obj/clibs.o obj/gohelper.a obj/tsgo.o obj/tsgomod.o obj/tsgowork.o
GO_DEPS = $(shell find go/ -type f -name '*.go')

DEP_FILES += obj/objclibs.d
OBJ_DEPS += obj/objclibs.o

.PHONY: all clean prep

all: build/bin/ide$(BINARY_SUFFIX) build/bin/buildcontext.go

prep:
	mkdir -p obj build/bin

clean:
	rm -rf obj/ build/bin/
	mkdir -p obj build/bin

build/bin/ide$(BINARY_SUFFIX): $(OBJ_DEPS) src/binaries.c obj/enums.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: src/%.cpp src/gohelper.h src/tstypes.hpp src/enums.hpp # Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

obj/enums.o: src/enums.cpp # Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

obj/objclibs.o: src/objclibs.mm
	$(CC) $(CFLAGS) -fobjc-arc -c -o $@ $<

obj/clibs.o: src/clibs.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

obj/tsgo.o: src/tsgo.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

obj/tsgomod.o: src/tree-sitter-go-mod/src/parser.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

obj/tsgowork.o: src/tree-sitter-go-work/src/parser.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

src/binaries.c: src/.cpcolors src/vert.glsl src/frag.glsl src/im.vert.glsl src/im.frag.glsl
	$(PYTHON) sh/create_binaries_c.py $^

COMMON_GOFLAGS = GOARCH=$(GOARCH) CC=clang CGO_CFLAGS="-mmacosx-version-min=10.12" CGO_LDFLAGS="-mmacosx-version-min=10.12"

obj/gohelper.a: $(GO_DEPS)
	$(COMMON_GOFLAGS) CGO_ENABLED=1 go build -ldflags "$(GOLDFLAGS)" -o $@ -buildmode=c-archive github.com/invrainbow/codeperfect/go/helper && \
		mv obj/gohelper.h src

src/gohelper.h: obj/gohelper.a

src/tstypes.hpp: src/tree-sitter-go/src/parser.c sh/generate_tstypes.py
	$(PYTHON) sh/generate_tstypes.py

src/enums.cpp: src/enums.hpp

src/enums.hpp: $(filter-out src/enums.hpp, $(wildcard src/*.hpp)) src/tstypes.hpp sh/generate_enums.py
	$(PYTHON) sh/generate_enums.py

build/bin/buildcontext.go: go/buildcontext/main.go
	cp go/buildcontext/main.go build/bin/buildcontext.go
