CC = gcc
CFLAGS = -Wall -g
LD = gcc
LDFLAGS = -g

SRC = mytar.c create.c extract.c list.c util.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean test

all: mytar

clean:
	rm -f $(OBJ) mytar

mytar: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

# default build for object files 
$(OBJ): %.o: %.c
