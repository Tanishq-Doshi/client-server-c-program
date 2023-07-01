# -*- MakeFile -*-

SRC_DIR = src
OBJ_DIR = lib
BIN_DIR = bin


CC       = gcc
CPPFLAGS = -MMD -MP
CFLAGS 	 = -I./src -O3 -Wall -Wextra 
LDLIBS   = -pthread


.PHONY: all clean


# List all source files here
SRCS_SERVER=$(wildcard $(SRC_DIR)/server.c)
SRCS_CLIENT=$(wildcard $(SRC_DIR)/client.c)

# Derive object file names from source file names
OBJS_SERVER=$(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS_SERVER))
OBJS_CLIENT=$(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS_CLIENT))

# Targets
all: server client

server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/server $(OBJS_SERVER)

client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/client $(OBJS_CLIENT)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN_DIR)/* **/*.log