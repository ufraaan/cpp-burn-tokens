CXX      := g++
CXXFLAGS := -std=c++17 -O3 -march=native -ffast-math -I/opt/homebrew/include -Isrc
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -pthread

SRCS     := src/main.cpp
OBJS     := $(SRCS:.cpp=.o)
TARGET   := form

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
