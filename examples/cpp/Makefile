CC=g++
DEPS=jsoncpp

# TODO The websocket null ptr token is a quick hack to compile in Trusty.
# See https://github.com/zaphoyd/websocketpp/issues/355
CCFLAGS=-g $(shell pkg-config --cflags $(DEPS)) -I/usr/local/include -std=c++0x -Wall -Werror -pedantic -D_WEBSOCKETPP_NULLPTR_TOKEN_=0
LDFLAGS=-g $(shell pkg-config --libs $(DEPS)) -lboost_system -lboost_random -lpthread

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



