VERSION := $(shell dpkg-parsechangelog | sed -n 's/^Version: //p')

CC      ?= gcc
CFLAGS  += -Wall -Wextra -D_REENTRANT -fpic

ifeq (,$(INSTALL_PROGRAM))
    INSTALL_PROGRAM := "install"
endif

prefix	= /usr
libdir 	= $(prefix)/lib
ifneq (,$(libarch))
# The `*-*' pattern matches to both hurd-i386 and e.g. i386-linux-gnu
marchpt = /*-*
endif
bindir	= $(prefix)/bin
mandir	= $(prefix)/share/man

all compile: datefudge datefudge.so datefudge.1

install: datefudge datefudge.so datefudge.1
	install -d $(DESTDIR)$(libdir)/$(libarch)/datefudge
	$(INSTALL_PROGRAM) -m 644 datefudge.so $(DESTDIR)$(libdir)/$(libarch)/datefudge/datefudge.so
	install -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 datefudge $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)/man1
	install -m 644 datefudge.1 $(DESTDIR)$(mandir)/man1

datefudge: datefudge.sh
datefudge.1: datefudge.man

datefudge datefudge.1:
	sed -e 's,@VERSION@,$(VERSION),g; s,@MULTIARCH_PATTERN@,$(marchpt),g; s,@LIBDIR@,$(libdir),g;' \
	< $< > $@

datefudge.so: datefudge.o
	$(CC) $(LDFLAGS) -o $@ -shared $< -ldl -lc

datefudge.o: datefudge.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f datefudge.o datefudge.so datefudge datefudge.1

# The first run may possibly fail when it's around midnight, that's why it's run twice.
test: compile
	@echo -n "Running a simple date test... "                                ;\
	export TZ=UTC								 ;\
	ret1=1; ret2=1                                                           ;\
	for i in 1 2; do                                                         \
	        export DATEFUDGE=`LC_ALL=C date --date='yesterday 0:00'  +%s`   ;\
	        dt=`LC_ALL=C LD_PRELOAD=$(CURDIR)/datefudge.so date --date=12:15 +%F.%T` ;\
	        exp="1970-01-02.12:15:00"                                       ;\
	        [ "$$dt" != "$$exp" ] || { echo "OK"; ret1=0; break; }          ;\
	        echo "failed: expected: $$exp, actual: $$dt"                    ;\
	        [ $$i = 2 ] || { echo -n "   retrying... "; sleep 2; }          ;\
	done                                                                    ;\
	echo -n "Running a simple perl localtime() test... "                    ;\
	pscr='@t=localtime(time);$$t[5]+=1900;$$t[4]++;printf "%04d-%02d-%02d\n",$$t[5],$$t[4],$$t[3];';\
	for i in 1 2; do                                                         \
	        export DATEFUDGE=`LC_ALL=C date --date='yesterday 0:00'  +%s`   ;\
	        dt=`LD_PRELOAD=$(CURDIR)/datefudge.so perl -e "$$pscr"`         ;\
	        exp="1970-01-02"                                                ;\
	        [ "$$dt" != "$$exp" ] || { echo "OK"; ret2=0; break; }          ;\
	        echo "failed: expected: $$exp, actual: $$dt"                    ;\
	        [ $$i = 2 ] || { echo -n "   retrying... "; sleep 2; }          ;\
	done                                                                    ;\
	exit `expr $$ret1 + $$ret2`;
