CC = clang++
BREW_X64 = arch --x86_64 /usr/local/Homebrew/bin/brew
BREW_ARM = /opt/homebrew/bin/brew

# CFLAGS = -std=c++17 -mavx -maes -w -MMD -MP
CFLAGS = -std=c++17 -w -MMD -MP
CFLAGS += -I$(shell $(BREW_ARM) --prefix glfw)/include
CFLAGS += -Itree-sitter

LDFLAGS = -ldl -framework OpenGL -framework Cocoa -framework IOKit
LDFLAGS += -framework CoreFoundation -framework Security  # for go
LDFLAGS += $(shell $(BREW_X64) --prefix glfw)/lib/libglfw3.a
LDFLAGS += $(shell $(BREW_X64) --prefix pcre)/lib/libpcre.a
LDFLAGS += $(shell $(BREW_ARM) --prefix glfw)/lib/libglfw3.a
LDFLAGS += $(shell $(BREW_ARM) --prefix pcre)/lib/libpcre.a
LDFLAGS += obj/gohelper.arm64.a

GOFLAGS =

ifeq (${RELEASE}, 1)
	CFLAGS += -arch x86_64 -arch arm64
	CFLAGS += -DRELEASE_MODE -O3
	GOFLAGS += -ldflags "-s -w"
	LDFLAGS += obj/gohelper.x64.a
else
	CFLAGS += -DDEBUG_MODE -g -O0
endif

SRC_FILES := $(filter-out tests.cpp, $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))

.PHONY: all clean build/launcher

all: build/bin/ide build/bin/init.vim build/bin/buildcontext.go

clean:
	rm -rf obj/ build/bin/

OBJ_DEPS = $(OBJ_FILES) obj/objclibs.o obj/clibs.o
ifeq (${RELEASE}, 1)
	OBJ_DEPS += obj/gohelper.arm64.a
	OBJ_DEPS += obj/gohelper.x64.a
endif

build/bin/test: $(filter-out obj/main.o, $(OBJ_DEPS)) obj/tests.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/bin/ide: $(OBJ_DEPS) cpcolors.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: %.cpp Makefile gohelper.h
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/tests.o: tests.cpp Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/objclibs.o: os_macos.mm
	$(CC) $(CFLAGS) -fobjc-arc -c -o $@ $<

obj/clibs.o: clibs.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

cpcolors.c: .cpcolors
	xxd -i .cpcolors cpcolors.c

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
