CC=g++
DEPS=jsoncpp

CCFLAGS=-g $(shell pkg-config --cflags $(DEPS)) -std=c++0x -Wall -Werror -pedantic
LDFLAGS=-g $(shell pkg-config --libs $(DEPS))

SRCS=pdal-session.cpp
OBJS=$(SRCS:.cpp=.o)

TARGET=pdal-session

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



