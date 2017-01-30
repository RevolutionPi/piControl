ifeq ($(CC),)
	CC = gcc
endif

IDIR =.
CFLAGS += -I. -I.. -std=c11

ODIR = obj

_DEPS = ../piControl.h piControlIf.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = piTest.o piControlIf.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

piTest: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: install clean

install:
	install -m 755 -d	$(DESTDIR)/usr/bin
	install -m 755 piTest	$(DESTDIR)/usr/bin
	ln -sf piTest		$(DESTDIR)/usr/bin/piControlReset
	install -m 755 -d	$(DESTDIR)/usr/share/man/man1
	install -m 644 piTest.1	$(DESTDIR)/usr/share/man/man1
	gzip -9 $(DESTDIR)/usr/share/man/man1/piTest.1

clean:
	rm -f $(ODIR)/*.o piTest
