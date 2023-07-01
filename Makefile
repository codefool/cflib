#
# Makefile for libcf
#
# This lives at https://github.com/codefool/libcf
#
INC_DIR := ./include
SRC_DIR := ./src
OBJ_DIR := ./obj
LIB_NAME := libcf

INSTALL_BASE_PATH := /usr/local/libcf
INSTALL_LIB_PATH  := $(INSTALL_BASE_PATH)/lib
INSTALL_INC_PATH  := $(INSTALL_BASE_PATH)/include


SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

CC := g++
CFLAGS := -g -std=c++2a -I$(INC_DIR)
ARC := ar
AFLAGS := rvs

$(LIB_NAME) : $(OBJ)
	$(ARC) $(AFLAGS) $@ $(OBJ)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_DIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $@
	$(ARC) $(AFLAGS) $(LIB_NAME) $@

clean:
	rm $(OBJ_DIR)/*.o $(LIB_NAME)

.PHONY : test dq_util dht_util

test:
	$(CC) $(CFLAGS) ./test/dstack_test.cpp -I. -L/usr/lib/x86_64-linux-gnu libcf -o dstack_test

dq_util:
	$(CC) $(CFLAGS) ./test/dq_util.cpp -I. -L/usr/lib/x86_64-linux-gnu libcf -o dq_util

dht_util:
	$(CC) $(CFLAGS) ./test/dht_util.cpp -I. -L/usr/lib/x86_64-linux-gnu libcf  -lpthread -o dht_util

install:
	mkdir -p $(INSTALL_LIB_PATH)
	mkdir -p $(INSTALL_INC_PATH)
	cp $(LIB_NAME) $(INSTALL_LIB_PATH)
	cp $(INC_DIR)/* $(INSTALL_INC_PATH)
