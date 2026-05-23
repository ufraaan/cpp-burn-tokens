CXX      := g++
CXXFLAGS := -std=c++17 -O2 -I/opt/homebrew/include -Isrc
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -pthread

SRCS     := src/main.cpp src/physics.cpp src/renderer.cpp src/audio.cpp
OBJS     := $(SRCS:.cpp=.o)
TARGET   := bhs

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
