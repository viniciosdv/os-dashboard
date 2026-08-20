CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -pthread
SRC = src/main.c src/engine_core.c src/memory.c src/metrics.c src/socket_server.c
BIN = bin/os_engine

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC) include/engine.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

run: all
	./$(BIN)

clean:
	rm -rf bin
