CXX = clang++

CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-deprecated-declarations -g
DEBUGFLAGS = -g -O0
# SANFLAGS = -fsanitize=thread
LDFLAGS = -pthread

TARGET = sim
SRCS = main.cpp globals.cpp umpire.cpp utility.cpp gantt.cpp caseworks.cpp bowler.cpp batsman.cpp fielder.cpp print.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp headers.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)

tsan: CXXFLAGS += $(DEBUGFLAGS) $(SANFLAGS)
tsan: LDFLAGS += $(SANFLAGS)
tsan: clean $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)