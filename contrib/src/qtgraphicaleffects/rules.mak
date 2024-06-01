# QtGraphicalEffects

QTGE_VERSION_MAJOR := 5.15
QTGE_VERSION := $(QTGE_VERSION_MAJOR).8
QTGE_URL := $(QT)/$(QTGE_VERSION_MAJOR)/$(QTGE_VERSION)/submodules/qtgraphicaleffects-everywhere-opensource-src-$(QTGE_VERSION).tar.xz

DEPS_qtgraphicaleffects += qtdeclarative $(DEPS_qtdeclarative)

ifdef HAVE_WIN32
PKGS += qtgraphicaleffects
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtgraphicaleffects
endif

$(TARBALLS)/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz:
	$(call download_pkg,$(QTGE_URL),qt)

.sum-qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz .sum-qtgraphicaleffects
	$(UNPACK)
	$(MOVE)

.qtgraphicaleffects: qtgraphicaleffects
	$(call qmake_toolchain, $<)
	cd $< && $(PREFIX)/lib/qt5/bin/qmake
	$(MAKE) -C $<
	$(MAKE) -C $< install
	touch $@
