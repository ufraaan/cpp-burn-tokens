CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra
LDFLAGS  :=
SRCS     := src/main.cpp src/huffman.cpp src/lzw.cpp
OBJS     := $(SRCS:.cpp=.o)
TARGET   := compress

.PHONY: all clean benchmark

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

benchmark: $(TARGET)
	@echo "=== Compressor Self-Test ==="
	./$(TARGET) -b $(TARGET)
	@echo ""
	@echo "=== Source Code ==="
	./$(TARGET) -b src/main.cpp
	@echo ""
	@echo "=== Binary (executable itself) ==="
	./$(TARGET) -b $(TARGET)

clean:
	rm -f src/*.o $(TARGET) *.cmp *.out
