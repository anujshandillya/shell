CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

TARGET = shell.out

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

cleanup:
	rm -f ${OBJ}

delete:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run