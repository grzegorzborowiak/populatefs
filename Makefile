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
CFLAGS?= -fPIC -Wall -O0 -fbuiltin -g
EXTRA_CFLAGS?= -DHAVE_GETOPT_H=1
INCLUDES?=
# -I../lib
LDFLAGS?= -L/usr/src/openwrt/trunk/staging_dir/host/lib -L/usr/lib
EXTRA_LDFLAGS?=
LIBS= -lext2fs -lm
EXTRA_LIBS?=
EXTRAVERSION?=

####################################################################

VERSION_MAJOR=0
VERSION_MINOR=9
VERSION=$(VERSION_MAJOR).$(VERSION_MINOR)
LIBPOPULATEFS=libpopulatefs
LIBPOPULATEFS_DEPENDS=src/debugfs.o src/util.o src/linklist.o src/mod_file.o src/mod_path.o src/log.o
LDADD_LIBPOPULATEFS= -lpopulatefs
OBJS=src/util.o src/log.o src/linklist.o src/debugfs.o src/mod_path.o src/mod_file.o

all: $(OBJS) src/main.o src/$(LIBPOPULATEFS).a src/$(LIBPOPULATEFS).so.$(VERSION) src/populatefs

src/populatefs: src/main.o src/$(LIBPOPULATEFS).a
	$(CC) $< -o $@ -L./src $(LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-Bstatic $(LDADD_LIBPOPULATEFS) -Wl,-Bdynamic $(LIBS) $(EXTRA_LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -DPOPULATEFS_VERSION="\"$(VERSION)\"" -DPOPULATEFS_EXTRAVERSION="\"$(EXTRAVERSION)\"" $(INCLUDES) -c $< -o $@

src/$(LIBPOPULATEFS).a: $(LIBPOPULATEFS_DEPENDS)
	$(AR)	rcsv $@	$(LIBPOPULATEFS_DEPENDS)
	$(RANLIB) $@

src/$(LIBPOPULATEFS).so.$(VERSION): $(LIBPOPULATEFS_DEPENDS)
	$(CC) -shared -Wl,-soname,$(LIBPOPULATEFS).so -Wl,-soname,$(LIBPOPULATEFS).so.$(VERSION_MAJOR) -o $@	$(LIBPOPULATEFS_DEPENDS)

install-bin: src/populatefs
	$(MKDIR) -p $(DESTDIR)/$(bindir)
	$(INSTALL) src/populatefs $(DESTDIR)/$(bindir)/
	$(STRIP) $(DESTDIR)/$(bindir)/populatefs

install-headers: src/log.h src/util.h src/linklist.h src/debugfs.h src/mod_path.h src/mod_file.h
	$(MKDIR) -p $(DESTDIR)/$(includedir)/populatefs
	$(CP) src/log.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/util.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/linklist.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/debugfs.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/mod_path.h $(DESTDIR)/$(includedir)/populatefs/
	$(CP) src/mod_file.h $(DESTDIR)/$(includedir)/populatefs/

install-libs: src/$(LIBPOPULATEFS).so.$(VERSION)
	$(MKDIR) -p $(DESTDIR)/$(libdir)
	$(RM) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION_MAJOR) \
		$(DESTDIR)/$(libdir)//$(LIBPOPULATEFS).so
	$(INSTALL) src/$(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/
	$(STRIP) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION)
	$(LN_S) $(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so.$(VERSION_MAJOR)
	$(LN_S) $(LIBPOPULATEFS).so.$(VERSION) $(DESTDIR)/$(libdir)/$(LIBPOPULATEFS).so

install-static-libs: src/$(LIBPOPULATEFS).a
	$(MKDIR) -p $(DESTDIR)/$(libdir)
	$(CP) src/$(LIBPOPULATEFS).a $(DESTDIR)/$(libdir)/

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
	$(RM) src/*.o src/*.a src/*.so* src/populatefs
