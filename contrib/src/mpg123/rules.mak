# mpg123
MPG123_VERSION := 1.30.1
MPG123_URL := $(SF)/mpg123/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"libmpg123"),)
PKGS_FOUND += mpg123
endif

# Same forced value as in VLC
MPG123CONF := CFLAGS="$(CFLAGS) -D_FILE_OFFSET_BITS=64"

MPG123CONF =
MPG123CONF += --with-default-audio=dummy --enable-buffer=no --enable-modules=no --disable-network

ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), armeabi-v7a)
MPG123CONF += --with-cpu=arm_fpu
else ifeq ($(ANDROID_ABI), arm64-v8a)
MPG123CONF += --with-cpu=aarch64
else
MPG123CONF += --with-cpu=generic_fpu
endif
endif

ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH), x86_64)
MPG123CONF += --with-cpu=generic_dither
endif
endif

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download_pkg,$(MPG123_URL),mpg123)

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	$(call pkg_static,"libmpg123.pc.in")
	$(MOVE)

.mpg123: mpg123
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MPG123CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@
