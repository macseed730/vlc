# protobuf
PROTOBUF_MAJVERSION := 3.4
PROTOBUF_REVISION := 1
PROTOBUF_VERSION := $(PROTOBUF_MAJVERSION).$(PROTOBUF_REVISION)
PROTOBUF_URL := $(GITHUB)/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

ifndef HAVE_TVOS
PKGS += protobuf protoc
PKGS_TOOLS += protoc
endif # !HAVE_TVOS
PKGS_ALL += protoc
ifeq ($(call need_pkg, "protobuf-lite = $(PROTOBUF_VERSION)"),)
PKGS_FOUND += protobuf
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += protoc
endif
endif

ifeq ($(shell $(HOST)-protoc --version 2>/dev/null | head -1 | sed s/'.* '// | cut -d '.' -f -2),$(PROTOBUF_MAJVERSION))
PKGS_FOUND += protoc
endif
ifeq ($(shell protoc --version 2>/dev/null | head -1 | sed s/'.* '// | cut -d '.' -f -2),$(PROTOBUF_MAJVERSION))
PKGS_FOUND += protoc
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

$(TARBALLS)/protoc-$(PROTOBUF_VERSION)-cpp.tar.gz: $(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz
	$(RM) -R "$@"
	cp "$<" "$@"

.sum-protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)

PROTOBUFVARS := DIST_LANG="cpp"
PROTOCVARS := DIST_LANG="cpp"

PROTOCCONF += --enable-static --disable-shared

.sum-protoc: .sum-protobuf
	touch $@

protoc: protoc-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protoc
	$(RM) -Rf $@ $(UNPACK_DIR) && mkdir -p $(UNPACK_DIR)
	tar $(TAR_VERBOSE)xzfo "$<" -C $(UNPACK_DIR) --strip-components=1
	# don't build benchmarks and conformance
	sed -i.orig 's, conformance benchmarks,,' "$(UNPACK_DIR)/Makefile.am"
	sed -i.orig 's, benchmarks/Makefile conformance/Makefile,,' "$(UNPACK_DIR)/configure.ac"
	# don't use gmock or any sub project to configure
	sed -i.orig 's,AC_CONFIG_SUBDIRS,dnl AC_CONFIG_SUBDIRS,' "$(UNPACK_DIR)/configure.ac"
	# force include <algorithm>
	sed -i.orig 's,#ifdef _MSC_VER,#if 1,' "$(UNPACK_DIR)/src/google/protobuf/repeated_field.h"
	$(APPLY) $(SRC)/protobuf/protobuf-no-mingw-pthread.patch
	$(MOVE)

.protoc: protoc
	$(RECONF)
	$(MAKEBUILDDIR)
	cd $(BUILD_DIR) && $(BUILDVARS) $(BUILD_SRC)/configure $(BUILDTOOLCONF) $(PROTOCVARS) $(PROTOCCONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	$(RM) -Rf $(UNPACK_DIR)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	# don't build benchmarks and conformance
	sed -i.orig 's, conformance benchmarks,,' "$(UNPACK_DIR)/Makefile.am"
	sed -i.orig 's, benchmarks/Makefile conformance/Makefile,,' "$(UNPACK_DIR)/configure.ac"
	# don't use gmock or any sub project to configure
	sed -i.orig 's,AC_CONFIG_SUBDIRS,dnl AC_CONFIG_SUBDIRS,' "$(UNPACK_DIR)/configure.ac"
	# don't build protoc
	sed -i.orig 's,bin_PROGRAMS,#bin_PROGRAMS,' "$(UNPACK_DIR)/src/Makefile.am"
	sed -i.orig 's,BUILT_SOURCES,#BUILT_SOURCES,' "$(UNPACK_DIR)/src/Makefile.am"
	sed -i.orig 's,libprotobuf-lite.la libprotobuf.la libprotoc.la,libprotobuf-lite.la libprotobuf.la,' "$(UNPACK_DIR)/src/Makefile.am"
	# force include <algorithm>
	sed -i.orig 's,#ifdef _MSC_VER,#if 1,' "$(UNPACK_DIR)/src/google/protobuf/repeated_field.h"
	$(APPLY) $(SRC)/protobuf/protobuf-no-mingw-pthread.patch
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(PROTOBUFVARS)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
