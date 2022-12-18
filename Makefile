#
# Makefile for cflib
#
# This lives at https://github.com/codefool/cflib
#
INCLUDE := -I./include -I./_include
SRC_DIR := ./src
OBJ_DIR := ./obj
LIB_NAME := libcf

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

CC := g++
CFLAGS := -g -std=c++2a $(INCLUDE)
ARC := ar
AFLAGS := rvs

$(LIB_NAME) : $(OBJ)
	$(ARC) $(AFLAGS) $@ $(OBJ)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm $(OBJ_DIR)/*.o $(LIB_NAME)