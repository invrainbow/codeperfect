CC = clang++
BREW_X64 = arch --x86_64 /usr/local/Homebrew/bin/brew
BREW_ARM = /opt/homebrew/bin/brew

CFLAGS = -std=c++17 -w -MMD -MP
# CFLAGS += -mavx -maes
CFLAGS += -Itree-sitter

LDFLAGS = -ldl -framework OpenGL -framework Cocoa -framework IOKit
LDFLAGS += -framework CoreFoundation -framework Security  # for go
LDFLAGS += -lfreetype -lharfbuzz -lfontconfig

ifeq (${ARCH}, x64)
	CFLAGS += -arch x86_64
	LDFLAGS += obj/gohelper.x64.a
	LDFLAGS += $(shell $(BREW_X64) --prefix pcre)/lib/libpcre.a
	LDFLAGS += -L$(shell $(BREW_X64) --prefix fontconfig)/lib
	LDFLAGS += -L$(shell $(BREW_X64) --prefix freetype)/lib
	LDFLAGS += -L$(shell $(BREW_X64) --prefix harfbuzz)/lib
else
	CFLAGS += -arch arm64
	LDFLAGS += obj/gohelper.arm64.a
	LDFLAGS += $(shell $(BREW_ARM) --prefix pcre)/lib/libpcre.a
	LDFLAGS += -L$(shell $(BREW_ARM) --prefix fontconfig)/lib
	LDFLAGS += -L$(shell $(BREW_ARM) --prefix freetype)/lib
	LDFLAGS += -L$(shell $(BREW_ARM) --prefix harfbuzz)/lib
endif

GOFLAGS =

ifeq (${RELEASE}, 1)
	CFLAGS += -DRELEASE_MODE -O3
	GOFLAGS += -ldflags "-s -w"
else
	CFLAGS += -DDEBUG_MODE -g -O0
endif

SEPARATE_SRC_FILES = tests.cpp enums.cpp
SRC_FILES := $(filter-out $(SEPARATE_SRC_FILES), $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))
DEP_FILES += obj/objclibs.d obj/clibs.d

.PHONY: all clean build/launcher

all: build/bin/ide build/bin/init.vim build/bin/buildcontext.go

prep:
	mkdir -p obj build/bin

clean:
	rm -rf obj/ build/bin/
	make prep

OBJ_DEPS = $(OBJ_FILES) obj/objclibs.o obj/clibs.o
ifeq (${RELEASE}, 1)
	OBJ_DEPS += obj/gohelper.arm64.a
	OBJ_DEPS += obj/gohelper.x64.a
endif

build/bin/test: $(filter-out obj/main.o, $(OBJ_DEPS)) obj/tests.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/bin/ide: $(OBJ_DEPS) binaries.c enums.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: %.cpp Makefile gohelper.h tstypes.hpp enums.hpp
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/enums.o: enums.cpp Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/tests.o: tests.cpp Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/objclibs.o: objclibs.mm
	$(CC) $(CFLAGS) -fobjc-arc -c -o $@ $<

obj/clibs.o: clibs.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

binaries.c: .cpcolors vert.glsl frag.glsl im.vert.glsl im.frag.glsl
	truncate -s 0 binaries.c
	for file in $^; do xxd -i $$file >> binaries.c; done

GOSTUFF_DIRS = $(shell find gostuff/ -type d)
GOSTUFF_FILES = $(shell find gostuff/ -type f -name '*')

obj/gohelper.x64.a: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; \
		CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build $(GOFLAGS) -o gohelper.x64.a -buildmode=c-archive ./helper; \
		mkdir -p ../obj; mv gohelper.x64.a ../obj; rm gohelper.x64.h

obj/gohelper.arm64.a: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; \
		CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build $(GOFLAGS) -o gohelper.arm64.a -buildmode=c-archive ./helper; \
		mkdir -p ../obj; mv gohelper.arm64.a ../obj; mv gohelper.arm64.h ../gohelper.h

gohelper.h: obj/gohelper.arm64.a

tstypes.hpp: tree-sitter-go/src/parser.c sh/generate_tstypes.py
	sh/generate_tstypes.py

enums.hpp: $(filter-out enums.hpp, $(wildcard *.hpp)) tstypes.hpp sh/generate_enums.py
	sh/generate_enums.py

build/launcher: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; \
		GOOS=darwin GOARCH=amd64 go build $(GOFLAGS) -o launcher.x64 ./cmd/launcher; \
		GOOS=darwin GOARCH=arm64 go build $(GOFLAGS) -o launcher.arm64 ./cmd/launcher; \
		lipo -create -output ../build/launcher launcher.x64 launcher.arm64; \
		rm launcher.x64 launcher.arm64

build/bin/buildcontext.go: gostuff/buildcontext/main.go
	cp gostuff/buildcontext/main.go build/bin/buildcontext.go

build/bin/init.vim: init.vim
	cp init.vim build/bin/init.vim
