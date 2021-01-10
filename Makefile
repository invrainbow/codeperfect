CC = g++

CFLAGS = -std=c++17 -fcompare-debug-second
CFLAGS += `pkg-config --cflags gtk+-3.0`
CFLAGS += `pkg-config --cflags glfw3`
CFLAGS += `pkg-config --cflags glew`

LIBS = -lstdc++fs -ldl
LIBS += `pkg-config --libs gtk+-3.0`
LIBS += `pkg-config --libs glfw3`
LIBS += `pkg-config --cflags --libs glew`

DEPS=`ls *.hpp *.h`

obj/%.o: %.c %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: obj/%.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

############### new stuff is below #################

SRC_DIR := .../src
OBJ_DIR := .../obj
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
LDFLAGS := ...
CPPFLAGS := ...
CXXFLAGS := ...

main.exe: $(OBJ_FILES)
   g++ $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
   g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
