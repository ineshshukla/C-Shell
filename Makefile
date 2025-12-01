# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
# The include path now points to our 'include' directory
CPPFLAGS = -Iinclude
LDFLAGS = 

# Target executable, placed in the parent directory (the project root)
TARGET = shell.out

# Tell 'make' to look for source files in the 'src' directory
VPATH = src

# Automatically find all .c files in the src directory
SRCS = $(wildcard src/*.c)

# Generate a list of object file names (e.g., main.o, prompt.o)
# These will be created in the current directory (shell/)
OBJS = $(notdir $(SRCS:.c=.o))

# The default 'all' target depends on the final executable
all: $(TARGET)

# Rule to link the executable from all the object files
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Generic rule to compile any .c source file into a .o object file.
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Rule to clean up generated files
clean:
	rm -f $(OBJS) $(TARGET)

# Declare 'all' and 'clean' as phony targets
.PHONY: all clean

