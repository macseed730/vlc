# lame

LAME_VERSION := 3.100
LAME_URL := $(SF)/lame/lame-$(LAME_VERSION).tar.gz

$(TARBALLS)/lame-$(LAME_VERSION).tar.gz:
	$(call download_pkg,$(LAME_URL),lame)

.sum-lame: lame-$(LAME_VERSION).tar.gz

lame: lame-$(LAME_VERSION).tar.gz .sum-lame
	$(UNPACK)
	$(APPLY) $(SRC)/lame/lame-forceinline.patch
	$(APPLY) $(SRC)/lame/sse.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/lame/struct-float-copy.patch
endif
	$(APPLY) $(SRC)/lame/lame-fix-i386-on-aarch64.patch
	# Avoid relying on iconv.m4 from gettext, when reconfiguring.
	# This is only used by the frontend which we disable.
	sed -i.orig 's/^AM_ICONV/#&/' $(UNPACK_DIR)/configure.in
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LAME_CONF := --disable-analyzer-hooks --disable-decoder --disable-gtktest --disable-frontend

ifdef WITH_OPTIMIZATION
LAME_CONF += CFLAGS="$(CFLAGS) -DNDEBUG"
endif

.lame: lame
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(LAME_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
