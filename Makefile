THIRDPARTY = thirdparty
MONGOOSE   = ${THIRDPARTY}/mongoose
JSONCPP    = ${THIRDPARTY}/jsoncpp/dist
LINENOISE  = ${THIRDPARTY}/linenoise

SOURCES    = src/cmdlog.cpp ${JSONCPP}/jsoncpp.cpp
LIBRARIES  =
INCLUDE    = -I${MONGOOSE} -I${JSONCPP}/json -I${LINENOISE}

CXXFLAGS   = -Wno-comment -Wno-deprecated-declarations -std=c++0x
# -std=c++11 | -std=c++0x | -std=gnu++11 # 0.y.z for jsoncpp, -Wall

CXX		   = g++
CC		   = gcc

dependencies: thirdparty mongoose jsoncpp linenoise

thirdparty:
	@echo "...thirdparty"
	@mkdir -p thirdparty
	@git submodule update --init --recursive >> /dev/null 2>&1

mongoose:
	@echo "...mongoose"
	@${CC} -Wall -W -Os -g -c ${MONGOOSE}/mongoose.c -o ${MONGOOSE}/mongoose.o

jsoncpp:
	@echo "...jsoncpp"
	@mkdir -p ${JSONCPP}
	@cd ${JSONCPP}/../ && git checkout 0.y.z >> /dev/null 2>&1 && python amalgamate.py >> /dev/null 2>&1
	@${CXX} -Wall -W -Os -g -c ${JSONCPP}/jsoncpp.cpp -o ${JSONCPP}/jsoncpp.o

linenoise:
	@echo "...linenoise"
	@${CC} -Wall -W -Os -g -c ${LINENOISE}/linenoise.c -o ${LINENOISE}/linenoise.o

cmdlog:
	${CXX} ${CXXFLAGS} ${LIBRARIES} ${INCLUDE} ${MONGOOSE}/mongoose.o ${LINENOISE}/linenoise.o ${SOURCES} -o cmdlog

all: dependencies cmdlog
	@echo "...cmdlog"

clean:
	rm -rf cmdlog
