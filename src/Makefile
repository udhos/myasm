
CC      = gcc
CFLAGS  = -Wall
LDFLAGS =

OBJ    = myassembler.o

TARGET = myassembler

.PHONY: default
default: $(TARGET)

.PHONY: clean
clean:
	rm -f *.o *~ $(TARGET)

.PHONY: build
build: clean default

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
