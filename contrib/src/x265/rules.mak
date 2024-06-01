# x265

#X265_GITURL := https://github.com/videolan/x265
X265_VERSION := 2.9
X265_SNAPURL := https://bitbucket.org/multicoreware/x265_git/downloads/x265_$(X265_VERSION).tar.gz

ifdef BUILD_ENCODERS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += x265
endif
endif
endif

ifeq ($(call need_pkg,"x265 >= 0.6"),)
PKGS_FOUND += x265
endif

$(TARBALLS)/x265-git.tar.xz:
	$(call download_git,$(X265_GITURL))

$(TARBALLS)/x265_$(X265_VERSION).tar.gz:
	$(call download_pkg,$(X265_SNAPURL),x265)

.sum-x265: x265_$(X265_VERSION).tar.gz

x265: x265_$(X265_VERSION).tar.gz .sum-x265
	$(UNPACK)
	$(APPLY) $(SRC)/x265/x265-ldl-linking.patch
	$(APPLY) $(SRC)/x265/x265-no-pdb-install.patch
	$(APPLY) $(SRC)/x265/x265-enable-detect512.patch
	$(call pkg_static,"source/x265.pc.in")
ifndef HAVE_WIN32
	$(APPLY) $(SRC)/x265/x265-pkg-libs.patch
endif
	$(MOVE)

X265_CONF := -DENABLE_SHARED=OFF -DENABLE_CLI=OFF

.x265: x265 toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) -S $</source $(X265_CONF)
	+$(CMAKEBUILD)
	sed -e s/'[^ ]*clang_rt[^ ]*'//g -i.orig "$(BUILD_DIR)/x265.pc"
	$(CMAKEINSTALL)
	touch $@
