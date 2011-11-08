CFLAGS := -Wall -Werror -pedantic-errors -std=c99 -g -Iinclude
PYTHON := python
PYTHONPATH := ${realpath .}
UTIL_SRC := src/util.c
CODEC_SRC := src/codec/codec.c
FRAMING_SRC := src/framing/framing.c
VALUE_SRC := src/value.c
UTIL_HDR := include/amp/util.h
VALUE_HDR := include/amp/value.h
ENGINE_SRC := src/engine/engine.c src/engine/connection.c src/engine/session.c \
	src/engine/link.c
DRIVER_SRC := src/driver.c

SRCS := ${UTIL_SRC} ${VALUE_SRC} ${FRAMING_SRC} ${CODEC_SRC} ${PROTOCOL_SRC} \
	${ENGINE_SRC} ${DRIVER_SRC}
OBJS := ${SRCS:.c=.o}
HDRS := ${UTIL_HDR} ${VALUE_HDR} \
	${FRAMING_SRC:src/framing/%.c=include/amp/%.h} \
	${CODEC_SRC:src/codec/%.c=include/amp/%.h} \
        src/protocol.h \
	include/amp/engine.h \
	src/codec/encodings.h

PROGRAMS := src/amp

all: ${PROGRAMS}

${PROGRAMS}: ${OBJS}

${OBJS}: ${HDRS}
${OBJS}: %.o: %.c

%.h: %.h.py
	PYTHONPATH=${PYTHONPATH} ${PYTHON} $< > $@

%.c: %.c.py
	PYTHONPATH=${PYTHONPATH} ${PYTHON} $< > $@

clean:
	rm -f ${PROGRAMS} ${OBJS} src/protocol.c src/protocol.h \
	src/codec/encodings.h src/*.pyc
