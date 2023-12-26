include config.mk

P=dictpopup
SRC = main.c popup.c util.c xlib.c deinflector.c unistr.c readsettings.c
OBJ = $(SRC:.c=.o)

all: options dictpopup

options:
	@echo popup build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

$(P): ${OBJ}
	$(CC) ${OBJ} ${LDFLAGS} -o $@ 

clean:
	rm -f dictpopup ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dictpopup ${DESTDIR}${PREFIX}/bin

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dictpopup

.PHONY: all options clean install uninstall
