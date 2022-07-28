CC = clang++

CFLAGS = -std=c++17 -MMD -MP -I. -Iimgui/ -ferror-limit=100 -w -Itree-sitter/lib/include
# CFLAGS += -mavx -maes

ifeq ($(TESTING_BUILD), 1)
	CFLAGS += -DTESTING_BUILD
endif

LDFLAGS =
LDFLAGS += obj/gohelper.a
LDFLAGS += -lfreetype -lharfbuzz -lpcre -lfontconfig
LDFLAGS += -lbrotlicommon-static -lbz2 -lbrotlidec-static

BINARY_NAME = ide

ifeq ($(OSTYPE), mac)
	CFLAGS += -DOSTYPE_MAC
	LDFLAGS += -ldl -lz -lexpat -lpng16

	frameworks = OpenGL Cocoa IOKit CoreFoundation Security
	LDFLAGS += $(foreach it, $(frameworks), -framework $(it))

	ifeq ($(shell sh/detect_m1), 1)
		CFLAGS += -arch arm64
		CFLAGS += -I./vcpkg/installed/arm64-osx/include
		LDFLAGS += -L./vcpkg/installed/arm64-osx/lib
		GOARCH = arm64
	else
		CFLAGS += -arch x86_64
		CFLAGS += -I./vcpkg/installed/x64-osx/include
		LDFLAGS += -L./vcpkg/installed/x64-osx/lib
		GOARCH = amd64
	endif
else ifeq ($(OSTYPE), windows)
	BINARY_NAME = ide.exe
	CFLAGS += -DOSTYPE_WINDOWS
	CFLAGS += -I./vcpkg/installed/x64-windows-static/include
	LDFLAGS += -L./vcpkg/installed/x64-windows-static/lib

	LDFLAGS += -lzlib -llibexpatMD -llibpng16
	LDFLAGS += -lopengl32 -ladvapi32 -lshlwapi -lole32
	LDFLAGS += -lpathcch -lshell32 -lwinmm -lws2_32 -lgdi32 -lshcore
	LDFLAGS += --for-linker "/IGNORE:4217"
	GOARCH = amd64
else ifeq ($(OSTYPE), linux)
	CFLAGS += -DOSTYPE_LINUX
endif

GOFLAGS =

ifeq ($(RELEASE), 1)
	CFLAGS += -O3
	GOFLAGS += -ldflags "-s -w"
else
	CFLAGS += -DDEBUG_BUILD -g -O0
endif

ifeq ($(OSTYPE), windows)
	PYTHON = python
else
	PYTHON = python3
endif

SEPARATE_SRC_FILES = tests.cpp enums.cpp
SRC_FILES := $(filter-out $(SEPARATE_SRC_FILES), $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))
DEP_FILES += obj/clibs.d
OBJ_DEPS = $(OBJ_FILES) obj/clibs.o obj/gohelper.a

# GOSTUFF_DEPS = $(shell find gostuff/ -type d)
GOSTUFF_DEPS = $(shell find gostuff/ -type f -name '*.go')
# GOSTUFF_DEPS += gostuff/

ifeq ($(OSTYPE), mac)
	DEP_FILES += obj/objclibs.d
	OBJ_DEPS += obj/objclibs.o
endif

.PHONY: all clean build/launcher

all: build/bin/$(BINARY_NAME) build/bin/init.vim build/bin/buildcontext.go

test: build/bin/test build/bin/init.vim build/bin/buildcontext.go

prep:
	mkdir -p obj build/bin

clean:
	rm -rf obj/ build/bin/
	mkdir -p obj build/bin

build/bin/test: $(filter-out obj/main.o, $(OBJ_DEPS)) obj/tests.o obj/enums.o binaries.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

build/bin/$(BINARY_NAME): $(OBJ_DEPS) binaries.c obj/enums.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

-include $(DEP_FILES)

$(OBJ_FILES): obj/%.o: %.cpp gohelper.h tstypes.hpp enums.hpp # Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/enums.o: enums.cpp # Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

obj/tests.o: tests.cpp # Makefile
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

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

obj/gohelper.a: $(GOSTUFF_DEPS)
	cd gostuff; \
		CC=clang CGO_ENABLED=1 go build $(GOFLAGS) -o gohelper.a -buildmode=c-archive ./helper && \
		(mkdir -p ../obj; mv gohelper.a ../obj; mv gohelper.h ..)

build/launcher: $(GOSTUFF_DEPS)
	cd gostuff; \
		go build $(GOFLAGS) -o ../build/launcher ./cmd/launcher

gohelper.h: obj/gohelper.a

tstypes.hpp: tree-sitter-go/src/parser.c sh/generate_tstypes.py
	$(PYTHON) sh/generate_tstypes.py

enums.cpp: enums.hpp

enums.hpp: $(filter-out enums.hpp, $(wildcard *.hpp)) tstypes.hpp sh/generate_enums.py
	$(PYTHON) sh/generate_enums.py

build/bin/buildcontext.go: gostuff/buildcontext/main.go
	cp gostuff/buildcontext/main.go build/bin/buildcontext.go

build/bin/init.vim: init.vim
	cp init.vim build/bin/init.vim

