CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -pedantic -O2 -g

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
HDR := $(wildcard src/*.h)
BIN := build/fs

.PHONY: all run test clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c $(HDR) | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

run: $(BIN)
	./$(BIN) disk.img

test: $(BIN)
	sh tests/run_tests.sh

clean:
	rm -rf build
