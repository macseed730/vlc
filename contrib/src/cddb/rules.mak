# CDDB
CDDB_VERSION := 1.3.2
CDDB_URL := $(SF)/libcddb/libcddb-$(CDDB_VERSION).tar.bz2

ifdef BUILD_NETWORK
ifdef BUILD_DISCS
PKGS += cddb
endif
endif
ifeq ($(call need_pkg,"libcddb"),)
PKGS_FOUND += cddb
endif

$(TARBALLS)/libcddb-$(CDDB_VERSION).tar.bz2:
	$(call download_pkg,$(CDDB_URL),cddb)

.sum-cddb: libcddb-$(CDDB_VERSION).tar.bz2

cddb: libcddb-$(CDDB_VERSION).tar.bz2 .sum-cddb
	$(UNPACK)
	$(APPLY) $(SRC)/cddb/cross.patch
	$(APPLY) $(SRC)/cddb/getenv-crash.patch
	$(APPLY) $(SRC)/cddb/cddb-no-alarm.patch
	$(APPLY) $(SRC)/cddb/fix-header-guards.patch
	$(APPLY) $(SRC)/cddb/no-gettext.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/cddb/win32-pkg.patch
endif
	$(MOVE)

DEPS_cddb = regex $(DEPS_regex) gettext $(DEPS_gettext)

CDDB_CONF := --without-iconv

CDDB_CFLAGS := $(CFLAGS) -D_BSD_SOCKLEN_T_=int
ifdef HAVE_WIN32
CDDB_CFLAGS += -DWIN32_LEAN_AND_MEAN
endif
CDDB_CONF += CFLAGS="$(CDDB_CFLAGS)"

.cddb: cddb
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(CDDB_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
