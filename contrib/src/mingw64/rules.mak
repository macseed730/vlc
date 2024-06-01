# winpthreads, dxvahd, winrt_headers, dcomp

MINGW64_VERSION := 10.0.0
MINGW64_URL := https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2/download
MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += winpthreads

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += winrt_headers
else  # !HAVE_WINSTORE
PKGS += d3d9 dcomp
endif # !HAVE_WINSTORE
PKGS += dxva dxvahd

ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += d3d9
endif # MINGW 8
ifeq ($(call mingw_at_least, 9), true)
ifdef HAVE_WINSTORE
PKGS_FOUND += winrt_headers
endif # HAVE_WINSTORE
endif # MINGW 9
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dxva
endif # MINGW 10
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dcomp
endif
ifeq ($(call mingw_at_least, 11), true)
PKGS_FOUND += dxvahd
endif # MINGW 11
endif # !HAVE_VISUALSTUDIO

HAVE_WINPTHREAD := $(shell $(CC) $(CFLAGS) -E -dM -include pthread.h - < /dev/null >/dev/null 2>&1 || echo FAIL)
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += winpthreads
endif

endif # HAVE_WIN32

PKGS_ALL += winpthreads winrt_headers d3d9 dxva dxvahd dcomp

$(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

.sum-mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2
# .sum-mingw64: mingw-w64-$(MINGW64_HASH).tar.xz

mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-mingw64
# mingw64: mingw-w64-$(MINGW64_HASH).tar.xz .sum-mingw64
	$(UNPACK)
	$(APPLY) $(SRC)/mingw64/0001-headers-Update-to-Wine-master-and-regenerate-H-from-.patch
	$(APPLY) $(SRC)/mingw64/0002-headers-dxvahd-Regenerate-H-from-IDL.patch
	$(MOVE)

.mingw64: mingw64
	touch $@

.sum-winpthreads: .sum-mingw64
	touch $@

.winpthreads: mingw64
	$(MAKEBUILDDIR)
	$(MAKECONFDIR)/mingw-w64-libraries/winpthreads/configure $(HOSTCONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@

.sum-winrt_headers: .sum-mingw64
	touch $@

MINGW_HEADERS_WINRT := \
    windows.foundation.h \
    windows.storage.h \
    windows.storage.streams.h \
    windows.system.threading.h \
    windows.foundation.collections.h \
    eventtoken.h \
    asyncinfo.h \
    windowscontracts.h

.winrt_headers: mingw64
	install -d "$(PREFIX)/include"
	install $(addprefix $</mingw-w64-headers/include/,$(MINGW_HEADERS_WINRT)) "$(PREFIX)/include"
	touch $@

.sum-dxvahd: .sum-mingw64
	touch $@

.dxvahd: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dxvahd.h "$(PREFIX)/include"
	touch $@

.sum-dcomp: .sum-mingw64
	touch $@

.dcomp: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dcomp.h "$(PREFIX)/include"
	touch $@

.sum-d3d9: .sum-mingw64
	touch $@

MINGW_HEADERS_D3D9 := d3d9.h d3d9caps.h

.d3d9: mingw64
	install -d "$(PREFIX)/include"
	install $(addprefix $</mingw-w64-headers/include/,$(MINGW_HEADERS_D3D9)) "$(PREFIX)/include"
	touch $@

.sum-dxva: .sum-mingw64
	touch $@

.dxva: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
	touch $@

