# libmpeg2

LIBMPEG2_VERSION := 0.5.1
LIBMPEG2_URL := http://libmpeg2.sourceforge.net/files/libmpeg2-$(LIBMPEG2_VERSION).tar.gz

ifdef GPL
PKGS += libmpeg2
endif
ifeq ($(call need_pkg,"libmpeg2"),)
PKGS_FOUND += libmpeg2
endif

$(TARBALLS)/libmpeg2-$(LIBMPEG2_VERSION).tar.gz:
	$(call download_pkg,$(LIBMPEG2_URL),libmpeg2)

.sum-libmpeg2: libmpeg2-$(LIBMPEG2_VERSION).tar.gz

libmpeg2: libmpeg2-$(LIBMPEG2_VERSION).tar.gz .sum-libmpeg2
	$(UNPACK)
	$(APPLY) $(SRC)/libmpeg2/libmpeg2-arm-pld.patch
	$(APPLY) $(SRC)/libmpeg2/libmpeg2-inline.patch
	$(APPLY) $(SRC)/libmpeg2/libmpeg2-mc-neon.patch
	$(UPDATE_AUTOCONFIG)
	cd $(UNPACK_DIR) && mv config.guess config.sub .auto
	$(MOVE)

LIBMPEG2_CONF := --without-x --disable-sdl

.libmpeg2: libmpeg2
	$(REQUIRE_GPL)
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(LIBMPEG2_CONF)
	+$(MAKEBUILD) -C libmpeg2
	+$(MAKEBUILD) -C include
	+$(MAKEBUILD) -C libmpeg2 install
	+$(MAKEBUILD) -C include install
	touch $@
