CFLAGS := -Wall -Werror -pedantic-errors -std=c99 -g -Iinclude
PYTHON := python
PYTHONPATH := ${PWD}
CODEC_SRC := src/codec/codec.c
FRAMING_SRC := src/framing/framing.c
TYPES_SRC := src/types/allocation.c  src/types/encoder.c  src/types/string.c \
	src/types/binary.c      src/types/list.c     src/types/symbol.c \
	src/types/box.c         src/types/map.c      src/types/type.c \
	src/types/decoder.c     src/types/scalars.c
PROTOCOL_SRC := src/protocol.c
ENGINE_SRC := src/engine/engine.c src/engine/connection.c src/engine/session.c \
	src/engine/link.c
DRIVER_SRC := src/driver.c

SRCS := ${TYPES_SRC} ${FRAMING_SRC} ${CODEC_SRC} ${PROTOCOL_SRC} ${ENGINE_SRC} \
	${DRIVER_SRC}
OBJS := ${SRCS:.c=.o}
HDRS := ${TYPES_SRC:src/types/%.c=include/amp/%.h} \
	${FRAMING_SRC:src/framing/%.c=include/amp/%.h} \
	${CODEC_SRC:src/codec/%.c=include/amp/%.h} \
	${PROTOCOL_SRC:%.c=%.h} \
	include/amp/engine.h \
	src/codec/encodings.h

PROGRAMS := src/amp src/test src/type_test

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
