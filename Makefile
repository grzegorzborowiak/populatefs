DESTDIR?=
prefix?=/usr
bindir?=${prefix}/bin
libdir?=${prefix}/lib
includedir?=${prefix}/include
CC?=gcc
AR?=ar
RANLIB?=ranlib
CP=cp
LN_S?=ln -s
RM?=rm -f
MKDIR?=mkdir
RMDIR?=rmdir
INSTALL?=install
STRIP?=strip -s
CFLAGS+= -fPIC -Wall -Werror
EXTRA_CFLAGS?= -DHAVE_GETOPT_H=1
INCLUDES?=
LDFLAGS+= -L/usr/lib -L./build
EXTRA_LDFLAGS?=
LIBS= -lext2fs
EXTRA_LIBS?=
EXTRAVERSION?=

##################################################
#                                                #
#  Don't go changing stuff further this point..  #
#  Unless, you really know what you are doing..  #
#                                                #
##################################################

VERSION_MAJOR=1
VERSION_MINOR=0
VERSION=$(VERSION_MAJOR).$(VERSION_MINOR)
LIBPOPULATEFS=libpopulatefs
LIBPOPULATEFS_DEPENDS=build/debugfs.o build/util.o build/linklist.o build/mod_file.o build/mod_path.o build/log.o
LDADD_LIBPOPULATEFS= -lpopulatefs
OBJS=build/util.o build/log.o build/linklist.o build/debugfs.o build/mod_path.o build/mod_file.o
HDRS=src/log.h src/util.h src/linklist.h src/debugfs.h src/mod_path.h src/mod_file.h

all: build/$(LIBPOPULATEFS).a build/$(LIBPOPULATEFS).so.$(VERSION) build/populatefs

build:
	mkdir $@

build/populatefs: build/main.o build/$(LIBPOPULATEFS).a $(HDRS)
	$(CC) $< -o $@ $(LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-Bstatic $(LDADD_LIBPOPULATEFS) -Wl,-Bdynamic $(LIBS) $(EXTRA_LIBS)

build/%.o: src/%.c build
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -DPOPULATEFS_VERSION="\"$(VERSION)\"" -DPOPULATEFS_EXTRAVERSION="\"$(EXTRAVERSION)\"" $(INCLUDES) -c $< -o $@

build/$(LIBPOPULATEFS).a: $(LIBPOPULATEFS_DEPENDS)
	$(AR)	rcsv $@	$(LIBPOPULATEFS_DEPENDS)
	$(RANLIB) $@

build/$(LIBPOPULATEFS).so.$(VERSION): $(LIBPOPULATEFS_DEPENDS)
	$(CC) -shared -Wl,-soname,$(LIBPOPULATEFS).so -Wl,-soname,$(LIBPOPULATEFS).so.$(VERSION_MAJOR) -o $@	$(LIBPOPULATEFS_DEPENDS)

install-bin: build/populatefs
	$(MKDIR) -p $(DESTDIR)/$(bindir)
	$(INSTALL) build/populatefs $(DESTDIR)/$(bindir)/
	$(STRIP) $(DESTDIR)/$(bindir)/populatefs

install-headers: $(HDRS)
	$(MKDIR) -p $(DESTDIR)/$(includedir)/populatefs
	$(CP) src/log.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/util.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/linklist.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/debugfs.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/mod_path.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/mod_file.h $(DESTDIR)/$(includedir)/populatefs/

install-libs: build/$(LIBPOPULATEFS).so.$(VERSION)
	$(MKDIR) -p $(DESTDIR)/$(libdir)
	$(RM) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION_MAJOR) \
		$(DESTDIR)/$(libdir)//$(LIBPOPULATEFS).so
	$(INSTALL) build/$(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/
	$(STRIP) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION)
	$(LN_S) $(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION_MAJOR)
	$(LN_S) $(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so

install-static-libs: build/$(LIBPOPULATEFS).a
	$(MKDIR) -p $(DESTDIR)/$(libdir)
	$(CP) build/$(LIBPOPULATEFS).a $(DESTDIR)/$(libdir)/

uninstall-bin:
	$(RM) $(DESTDIR)/$(bindir)/populatefs

uninstall-headers:
	$(RM) $(DESTDIR)/$(includedir)/populatefs/log.h
	$(RM) $(DESTDIR)/$(includedir)/populatefs/util.h
	$(RM) $(DESTDIR)/$(includedir)/populatefs/linklist.h
	$(RM) $(DESTDIR)/$(includedir)/populatefs/debugfs.h
	$(RM) $(DESTDIR)/$(includedir)/populatefs/mod_path.h
	$(RM) $(DESTDIR)/$(includedir)/populatefs/mod_file.h
	$(RMDIR) --ignore-fail-on-non-empty $(DESTDIR)/$(includedir)/populatefs

uninstall-libs:
	$(RM) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so*
	$(RMDIR) --ignore-fail-on-non-empty $(DESTDIR)/$(libdir)/populatefs

uninstall-static-libs:
	$(RM) $(DESTDIR)/$(libdir)/populatefs/$(LIBPOPULATEFS).a
	$(RMDIR) --ignore-fail-on-non-empty $(DESTDIR)/$(libdir)/populatefs

install: all install-bin install-headers install-libs

uninstall: uninstall-bin uninstall-libs

clean:
	$(RM) build/*
