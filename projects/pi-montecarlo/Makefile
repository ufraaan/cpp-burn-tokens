CXX      := clang++
CXXFLAGS := -std=c++17 -O3
LDFLAGS  := -framework Metal -framework Foundation
SRCS     := src/main.mm
OBJS     := src/main.o
TARGET   := pi-montecarlo

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/main.o: src/main.mm
	$(CXX) $(CXXFLAGS) -x objective-c++ -fobjc-arc -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
