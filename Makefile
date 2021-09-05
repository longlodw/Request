CC=g++
SRC=src
OBJ=obj
BIN=bin

CPP_FILES := $(wildcard $(SRC)/*.cpp)
H_FILES := $(wildcard $(SRC)/*.h)
O_FILES := $(wildcard ${OBJ}/*.o)

librequest.so: ${O_FILES}
	${CC} -shared $^ -o ${BIN}/$@

%.o: ${CPP_FILES} ${H_FILES} Request.h
	${CC} -std=c++17 -c -fpic -o ${OBJ}/$@ ${CPP_FILES}