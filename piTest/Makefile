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
	if [ -e ../picontrol_ioctl.4 ] ; then				\
		install -m 755 -d $(DESTDIR)/usr/share/man/man4	;	\
		install -m 644 ../picontrol_ioctl.4			\
			$(DESTDIR)/usr/share/man/man4 ;			\
	fi

clean:
	rm -f $(ODIR)/*.o piTest
