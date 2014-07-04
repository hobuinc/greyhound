CC=g++
DEPS=jsoncpp

# TODO The websocket null ptr token is a quick hack to compile in Trusty.
# See https://github.com/zaphoyd/websocketpp/issues/355
CCFLAGS=-g $(shell pkg-config --cflags $(DEPS)) -I/usr/local/include -std=c++0x -Wall -Werror -pedantic -D_WEBSOCKETPP_NULLPTR_TOKEN_=0
LDFLAGS=-g $(shell pkg-config --libs $(DEPS)) -lboost_system -lboost_random -lpthread

SRCS=get-points.cpp put-pipeline.cpp
OBJS=$(SRCS:.cpp=.o)

PUT_TGT=put-pipeline
PUT_OBJ=put-pipeline.o
PUT_SRC=put-pipeline.cpp
GET_TGT=get-points
GET_OBJ=get-points.o
GET_SRC=get-points.cpp

TARGETS=$(PUT_TGT) $(GET_TGT)

all: $(TARGETS)

vars:
	@echo CCFLAGS: $(CCFLAGS)
	@echo LDFLAGS: $(LDFLAGS)
	@echo Targets: $(TARGETS)
	@echo Sources: $(SRCS)
	@echo Objects: $(OBJS)

$(PUT_TGT): $(PUT_OBJ)
	$(CC) -o $(PUT_TGT) $(PUT_OBJ) $(LDFLAGS)

$(GET_TGT): $(GET_OBJ)
	$(CC) -o $(GET_TGT) $(GET_OBJ) $(LDFLAGS)

$(PUT_OBJ) : $(PUT_SRC)
	$(CC) -c $(CCFLAGS) $(PUT_SRC) -o $(PUT_OBJ)

$(GET_OBJ) : $(GET_SRC)
	$(CC) -c $(CCFLAGS) $(GET_SRC) -o $(GET_OBJ)

clean:
	rm -rf $(OBJS) $(TARGETS)

