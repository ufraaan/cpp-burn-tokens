CXX      := g++
CXXFLAGS := -std=c++17 -O2 -I/opt/homebrew/include -Isrc
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -framework OpenGL -framework Cocoa

SRCS     := src/main.cpp src/terrain.cpp
OBJS     := $(SRCS:.cpp=.o)
TARGET   := voxel

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
