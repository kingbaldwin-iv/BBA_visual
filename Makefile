TARGET = vis

# Compiler
CC = gcc

# Source files
SRC = main.c


CFLAGS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -L/opt/homebrew/lib -lraylib -lwebsockets
INCLUDES = -I/usr/local/include -I/opt/homebrew/include

# Build target
$(TARGET): $(SRC)
        $(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(INCLUDES)

run: $(TARGET)
        ./$(TARGET)
clean:
        rm -f $(TARGET)

default: $(TARGET) run
