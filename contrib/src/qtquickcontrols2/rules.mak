# QtQuickControls 2

QTQC2_VERSION_MAJOR := 5.15
QTQC2_VERSION := $(QTQC2_VERSION_MAJOR).8
QTQC2_URL := $(QT)/$(QTQC2_VERSION_MAJOR)/$(QTQC2_VERSION)/submodules/qtquickcontrols2-everywhere-opensource-src-$(QTQC2_VERSION).tar.xz

ifdef HAVE_WIN32
PKGS += qtquickcontrols2
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtquickcontrols2
endif
# QtQuickControl(1) doesn't provide a .pc

DEPS_qtquickcontrols2 = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz:
	$(call download_pkg,$(QTQC2_URL),qt)

.sum-qtquickcontrols2: qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz

qtquickcontrols2: qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz .sum-qtquickcontrols2
	$(UNPACK)
	$(MOVE)

QUICK_CONTROL_CONFIG := \
    -no-feature-quicktemplates2-multitouch \
    -no-feature-quickcontrols2-universal \
    -no-feature-quickcontrols2-material \
    -no-feature-quickcontrols2-imagine

.qtquickcontrols2: qtquickcontrols2
	$(call qmake_toolchain, $<)
	cd $< && $(PREFIX)/lib/qt5/bin/qmake -- $(QUICK_CONTROL_CONFIG)
	# Make && Install libraries
	$(MAKE) -C $<
ifndef HAVE_CROSS_COMPILE
	cd $<; for i in QtQuickControls2 QtQuickTemplates2; do \
		sed -i -e 's,"../../../../../src,"../src,g' include/$$i/$(QTQC2_VERSION)/$$i/private/*.h; done
endif
	$(MAKE) -C $< install
	touch $@
