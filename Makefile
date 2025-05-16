TARGET=src/proj2

CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread -lrt

default: all

all: $(TARGET)

$(TARGET): $(TARGET).c
	gcc $(CFLAGS) $(TARGET).c -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf *.o proj2
