CC = clang++
BREW_X64 = arch --x86_64 /usr/local/Homebrew/bin/brew
BREW_ARM = /opt/homebrew/bin/brew
BREW = $(if $(filter $(shell uname -m), arm64), $(BREW_ARM), $(BREW_X64))

# CFLAGS = -std=c++17 -mavx -maes -w -MMD -MP
CFLAGS = -std=c++17 -w -MMD -MP
CFLAGS += -I$(shell $(BREW) --prefix glfw)/include
CFLAGS += -Itree-sitter/src -Itree-sitter/include
CFLAGS += $(if $(filter $(shell uname -m), arm64), -DCPU_ARM64,)
CFLAGS += -arch x86_64 -arch arm64

ifeq (${RELEASE}, 1)
	GOFLAGS += -ldflags "-s -w"
	CFLAGS += -DRELEASE_MODE -O3
else
	CFLAGS += -DDEBUG_MODE -g -O0
endif

LDFLAGS = -ldl -framework OpenGL -framework Cocoa -framework IOKit
LDFLAGS += -framework CoreFoundation -framework Security  # for go
LDFLAGS += $(shell $(BREW_X64) --prefix glfw)/lib/libglfw3.a
LDFLAGS += $(shell $(BREW_X64) --prefix pcre)/lib/libpcre.a
LDFLAGS += $(shell $(BREW_ARM) --prefix glfw)/lib/libglfw3.a
LDFLAGS += $(shell $(BREW_ARM) --prefix pcre)/lib/libpcre.a
LDFLAGS += obj/gohelper.a

SRC_FILES := $(filter-out tests.cpp, $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))

.PHONY: all clean

all: build/bin/ide build/launcher build/bin/dynamic_helper.go build/bin/int.vim

clean:
	rm -rf obj/ build/bin/

build/bin/test: $(filter-out obj/main.o, $(OBJ_FILES)) obj/gohelper.a obj/objclibs.o obj/clibs.o obj/tests.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/bin/ide: $(OBJ_FILES) obj/gohelper.a obj/objclibs.o obj/clibs.o cpcolors.c
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

# obj/glew.o: glew.c
# 	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

cpcolors.c: .cpcolors
	xxd -i .cpcolors cpcolors.c

GOSTUFF_DIRS = $(shell find gostuff/ -type d)
GOSTUFF_FILES = $(shell find gostuff/ -type f -name '*')

obj/gohelper.a: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; \
		CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build $(GOFLAGS) -o gohelper.x64.a -buildmode=c-archive ./helper; \
		CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build $(GOFLAGS) -o gohelper.arm64.a -buildmode=c-archive ./helper; \
		mkdir -p ../obj; \
		lipo -create -output ../obj/gohelper.a gohelper.x64.a gohelper.arm64.a; \
		cp gohelper.x64.h ../gohelper.h; \
		rm gohelper.*.a gohelper.*.h

gohelper.h: obj/gohelper.a

build/launcher: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; \
		GOOS=darwin GOARCH=amd64 go build $(GOFLAGS) -o launcher.x64 ./cmd/launcher; \
		GOOS=darwin GOARCH=arm64 go build $(GOFLAGS) -o launcher.arm64 ./cmd/launcher; \
		lipo -create -output ../build/launcher launcher.x64 launcher.arm64; \
		rm launcher.x64 launcher.arm64

build/bin/dynamic_helper.go: dynamic_helper.go
	cp dynamic_helper.go build/bin/dynamic_helper.go

build/bin/int.vim: init.vim
	cp init.vim build/bin/init.vim
