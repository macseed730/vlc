# zvbi

ZVBI_VERSION := 0.2.35
ZVBI_URL := $(SF)/zapping/zvbi-$(ZVBI_VERSION).tar.bz2

PKGS += zvbi
ifeq ($(call need_pkg,"zvbi-0.2"),)
PKGS_FOUND += zvbi
endif

$(TARBALLS)/zvbi-$(ZVBI_VERSION).tar.bz2:
	$(call download_pkg,$(ZVBI_URL),zvbi)

.sum-zvbi: zvbi-$(ZVBI_VERSION).tar.bz2

zvbi: zvbi-$(ZVBI_VERSION).tar.bz2 .sum-zvbi
	$(UNPACK)
	$(APPLY) $(SRC)/zvbi/zvbi-ssize_max.patch
	$(APPLY) $(SRC)/zvbi/zvbi-ioctl.patch
	$(APPLY) $(SRC)/zvbi/zvbi-fix-static-linking.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/zvbi/zvbi-win32.patch
	$(APPLY) $(SRC)/zvbi/zvbi-win32-undefined.patch
endif
	$(APPLY) $(SRC)/zvbi/zvbi-fix-clang-support.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/zvbi/zvbi-android.patch
endif
	$(MOVE)

DEPS_zvbi = png $(DEPS_png) iconv $(DEPS_iconv)

ZVBICONF := \
	--disable-dvb --disable-bktr \
	--disable-nls --disable-proxy \
	--without-doxygen

ifdef HAVE_WIN32
DEPS_zvbi += winpthreads $(DEPS_winpthreads)
endif

.zvbi: zvbi
	$(UPDATE_AUTOCONFIG)
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(ZVBICONF)
	+$(MAKEBUILD) -C src
	+$(MAKEBUILD) SUBDIRS=.
	+$(MAKEBUILD) -C src install
	+$(MAKEBUILD) SUBDIRS=. install
	sed -i.orig -e "s/\/[^ ]*libiconv.a/-liconv/" $(PREFIX)/lib/pkgconfig/zvbi-0.2.pc
	touch $@
