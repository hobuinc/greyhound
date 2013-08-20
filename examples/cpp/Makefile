CC=g++
DEPS=jsoncpp

CCFLAGS=-g $(shell pkg-config --cflags $(DEPS)) -I/usr/local/include -std=c++0x -Wall -Werror -pedantic
LDFLAGS=-g $(shell pkg-config --libs $(DEPS)) -lboost_system -lboost_random

SRCS=get-points.cpp
OBJS=$(SRCS:.cpp=.o)

TARGET=get-points

all: $(TARGET)

vars:
	@echo CCFLAGS: $(CCFLAGS)
	@echo LDFLAGS: $(LDFLAGS)
	@echo Sources: $(SRCS)
	@echo Objects: $(OBJS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $< $(LDFLAGS)

%.o : %.cpp
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET)



