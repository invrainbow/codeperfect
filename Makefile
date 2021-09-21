CC = clang++

CFLAGS = -std=c++17 -mavx -maes -w -MMD -MP
CFLAGS += `pkg-config --cflags glfw3 glew`
CFLAGS += -Itree-sitter/src -Itree-sitter/include -I/opt/homebrew/include
CFLAGS += $(if $(filter $(shell uname -m), arm64), -DCPU_ARM64,)

ifeq (${RELEASE}, 1)
	GOFLAGS += -ldflags "-s -w"
	CFLAGS += -DRELEASE_MODE -O3
else
	CFLAGS += -DDEBUG_MODE -g
endif

$(info $$CFLAGS is [${CFLAGS}])

LDFLAGS = -ldl -lcwalk -lpcre -framework OpenGL -framework Cocoa -L/opt/homebrew/lib
LDFLAGS += `pkg-config --libs glfw3 glew`

SRC_FILES := $(filter-out tests.cpp, $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))

.PHONY: all

all: build/bin/ide build/bin/gohelper.dylib build/launcher build/bin/dynamic_helper.go build/bin/int.vim

build/bin/test: $(filter-out obj/main.o, $(OBJ_FILES)) obj/objclibs.o obj/clibs.o obj/tests.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/bin/ide: $(OBJ_FILES) obj/objclibs.o obj/clibs.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: %.cpp Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/tests.o: tests.cpp Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/objclibs.o: os_macos.mm
	$(CC) $(CFLAGS) -fobjc-arc -c -o $@ $<

obj/clibs.o: clibs.c
	clang $(CFLAGS) -std=gnu99 -fPIC -c -o $@ $<

GOSTUFF_DIRS = $(shell find gostuff/ -type d)
GOSTUFF_FILES = $(shell find gostuff/ -type f -name '*')

build/bin/gohelper.dylib: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; go build $(GOFLAGS) -o ../build/bin/gohelper.dylib -buildmode=c-shared github.com/invrainbow/codeperfect/gostuff/cmd/helper

build/launcher: gostuff/ $(GOSTUFF_DIRS) $(GOSTUFF_FILES)
	cd gostuff; go build $(GOFLAGS) -o ../build/launcher github.com/invrainbow/codeperfect/gostuff/cmd/launcher

build/bin/dynamic_helper.go: dynamic_helper.go
	cp dynamic_helper.go build/bin/dynamic_helper.go

build/bin/int.vim: init.vim
	cp init.vim build/bin/init.vim
