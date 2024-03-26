# Define compiler
CC=g++

# Compiler flags
CFLAGS=-std=c++11 -g

# Target executable name
TARGET=database

# Source files
SOURCES=database.cpp

# Object files
OBJECTS=$(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Explicit rule for object files to ensure CFLAGS are applied
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

clean:
	rm -f $(TARGET) $(OBJECTS) *.data *.log

.PHONY: all clean
