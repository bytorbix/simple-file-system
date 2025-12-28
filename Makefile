CC = gcc
# -Iinclude tells the compiler to look in your 'include' folder for .h files
# -fsanitize=address is the "Heap Buffer" detector we discussed
CFLAGS = -Wall -Wextra -g -fsanitize=address -Iinclude

# Linker flags (libraries)
LIBS = -lm

# Your source files with their specific paths
SRCS = main.c src/library/fs.c src/library/disk.c

# This automatically creates a list of .o files based on the SRCS list
OBJS = $(SRCS:.c=.o)

# The name of your final executable
TARGET = mfs

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# This generic rule compiles .c files into .o files
# It works for main.c AND src/library/fs.c automatically
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	# This cleans up the main folder and the subfolders
	rm -f $(OBJS) $(TARGET)