CC = clang++

CFLAGS = -std=c++17 -w -MMD -MP
# CFLAGS += -mavx -maes
CFLAGS += -Itree-sitter

LDFLAGS += obj/gohelper.a

ifeq ($(OSTYPE), mac)
	LDFLAGS = -ldl -framework OpenGL -framework Cocoa -framework IOKit
	LDFLAGS += -framework CoreFoundation -framework Security # for go
	LDFLAGS += -lfreetype -lharfbuzz -lfontconfig

	LDFLAGS += $(shell brew --prefix pcre)/lib/libpcre.a
	LDFLAGS += -L$(shell brew --prefix fontconfig)/lib
	LDFLAGS += -L$(shell brew --prefix freetype)/lib
	LDFLAGS += -L$(shell brew --prefix harfbuzz)/lib

	M1 = $(shell sh/detect_m1)
	ifeq ($(M1), 1)
		CFLAGS += -arch arm64
		GOARCH = arm64
	else
		CFLAGS += -arch x86_64
		GOARCH = amd64
	endif
else ifeq ($(OSTYPE), windows)
	GOARCH = amd64
endif

GOFLAGS =

ifeq ($(RELEASE), 1)
	CFLAGS += -DRELEASE_MODE -O3
	GOFLAGS += -ldflags "-s -w"
else
	CFLAGS += -DDEBUG_MODE -g -O0
endif

GOSTUFF_DEPS = $(shell find gostuff/ -type d)
GOSTUFF_DEPS += $(shell find gostuff/ -type f -name '*')

ifeq ($(OSTYPE), windows)
	SHELL_SUFFIX := .bat
	PYTHON = python
	EXEC = call
else
	SHELL_SUFFIX = _$(OSTYPE)
	PYTHON = python3
	EXEC = bash
endif

SEPARATE_SRC_FILES = tests.cpp enums.cpp
SRC_FILES := $(filter-out $(SEPARATE_SRC_FILES), $(wildcard *.cpp))
OBJ_FILES = $(patsubst %.cpp,obj/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst %.cpp,obj/%.d,$(SRC_FILES))
DEP_FILES += obj/clibs.d
OBJ_DEPS = $(OBJ_FILES) obj/clibs.o obj/gohelper.a

ifeq ($(OSTYPE), mac)
	DEP_FILES += obj/objclibs.d 
	OBJ_DEPS += obj/objclibs.o 
endif
 
.PHONY: all clean build/launcher

all: build/bin/ide build/bin/init.vim build/bin/buildcontext.go

ifeq ($(OSTYPE), windows)
prep:
	md obj
	md build\bin
clean:
	del /s /q obj
	del /s /q build\bin
	make prep
else
prep:
	mkdir -p obj build/bin
clean:
	rm -rf obj/ build/bin/
	make prep
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

ifeq ($(OSTYPE), windows)
	PIC_FLAGS =
else
	PIC_FLAGS = -fPIC
endif

obj/clibs.o: clibs.c
	clang $(CFLAGS) -std=gnu99 $(PIC_FLAGS) -c -o $@ $<

binaries.c: .cpcolors vert.glsl frag.glsl im.vert.glsl im.frag.glsl
	$(PYTHON) sh/create_binaries_c.py $^

obj/gohelper.a: gostuff/ $(GOSTUFF_DEPS)
	cd gostuff; \
		CGO_ENABLED=1 go build "$@" -o gohelper.a -buildmode=c-archive helper; \
		mkdir -p ../obj; \
		mv gohelper.a ../obj; \
		rm gohelper.h

build/launcher: gostuff/ $(GOSTUFF_DEPS)
	cd gostuff; \
		go build "$@"  -o ../build/launcher cmd/launcher; \

gohelper.h: obj/gohelper.a

tstypes.hpp: tree-sitter-go/src/parser.c sh/generate_tstypes.py
	$(PYTHON) sh/generate_tstypes.py

enums.hpp: $(filter-out enums.hpp, $(wildcard *.hpp)) tstypes.hpp sh/generate_enums.py
	$(PYTHON) sh/generate_enums.py

build/bin/buildcontext.go: gostuff/buildcontext/main.go
	cp gostuff/buildcontext/main.go build/bin/buildcontext.go

build/bin/init.vim: init.vim
	cp init.vim build/bin/init.vim
