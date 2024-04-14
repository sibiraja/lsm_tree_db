# Define compiler
CC=g++

# Compiler flags
CFLAGS=-std=c++11 -g -Wall -Wextra -pedantic -Wno-missing-field-initializers -ggdb -D_FILE_OFFSET_BITS=64 -pthread

# Target executable names
TARGET1=database
# TARGET2=put_test
# TARGET3=get_test

# Source files for each executable
SOURCES1=database.cpp lsm.cpp
# SOURCES2=put_test.cpp
# SOURCES3=get_test.cpp

# Object files for each executable
OBJECTS1=$(SOURCES1:.cpp=.o)
# OBJECTS2=$(SOURCES2:.cpp=.o)
# OBJECTS3=$(SOURCES3:.cpp=.o)

# Default target
all: $(TARGET1)

# Explicit rule for object files to ensure CFLAGS are applied
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET1): $(OBJECTS1)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJECTS1)

# $(TARGET2): $(OBJECTS2)
# 	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJECTS2)

# $(TARGET3): $(OBJECTS3)
# 	$(CC) $(CFLAGS) -o $(TARGET3) $(OBJECTS3)

clean:
	rm -f $(TARGET1) $(OBJECTS1) *.data

.PHONY: all clean
