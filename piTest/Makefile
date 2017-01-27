IDIR =.
CC = gcc
CFLAGS += -I. -I..

ODIR = obj

_DEPS = ../piControl.h piControlIf.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = piTest.o piControlIf.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

piTest: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o
