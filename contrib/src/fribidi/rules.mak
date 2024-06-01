# FRIBIDI
FRIBIDI_VERSION := 1.0.12
FRIBIDI_URL := $(GITHUB)/fribidi/fribidi/releases/download/v$(FRIBIDI_VERSION)/fribidi-$(FRIBIDI_VERSION).tar.xz

PKGS += fribidi
ifeq ($(call need_pkg,"fribidi"),)
PKGS_FOUND += fribidi
endif

$(TARBALLS)/fribidi-$(FRIBIDI_VERSION).tar.xz:
	$(call download_pkg,$(FRIBIDI_URL),fribidi)

.sum-fribidi: fribidi-$(FRIBIDI_VERSION).tar.xz

fribidi: fribidi-$(FRIBIDI_VERSION).tar.xz .sum-fribidi
	$(UNPACK)
	$(MOVE)

# FIXME: DEPS_fribidi = iconv $(DEPS_iconv)
.fribidi: fribidi crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) -Ddocs=false -Dbin=false -Dtests=false
	+$(MESONBUILD)
	touch $@
