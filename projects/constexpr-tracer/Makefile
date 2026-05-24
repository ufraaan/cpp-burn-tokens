CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -I/opt/homebrew/include
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -framework Metal -framework Foundation
SRCS     := src/main.cpp src/gpu_tracer.mm
OBJS     := src/main.o src/gpu_tracer.o
TARGET   := cpptracer

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/main.o: src/main.cpp src/gpu_tracer.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/gpu_tracer.o: src/gpu_tracer.mm src/gpu_tracer.hpp
	$(CXX) $(CXXFLAGS) -x objective-c++ -fobjc-arc -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
