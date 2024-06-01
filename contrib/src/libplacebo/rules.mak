# libplacebo

PLACEBO_VERSION := 5.229.1
PLACEBO_ARCHIVE = libplacebo-v$(PLACEBO_VERSION).tar.gz
PLACEBO_URL := https://code.videolan.org/videolan/libplacebo/-/archive/v$(PLACEBO_VERSION)/$(PLACEBO_ARCHIVE)

PLACEBOCONF := -Dpython-path=$(PYTHON_VENV)/bin/python3 \
	-Dvulkan-registry=${PREFIX}/share/vulkan/registry/vk.xml \
	-Dglslang=enabled \
	-Dshaderc=disabled \
	-Ddemos=false \
	-Dtests=false

DEPS_libplacebo = glad $(DEPS_glad) jinja $(DEPS_jinja) glslang $(DEPS_glslang) vulkan-headers $(DEPS_vulkan-headers)
ifndef HAVE_WINSTORE
PKGS += libplacebo
endif
ifeq ($(call need_pkg,"libplacebo >= 4.157"),)
PKGS_FOUND += libplacebo
endif

ifdef HAVE_WIN32
DEPS_libplacebo += winpthreads $(DEPS_winpthreads)
endif

# We don't want vulkan on darwin for now
ifndef HAVE_DARWIN_OS
ifndef HAVE_EMSCRIPTEN
DEPS_libplacebo += vulkan-loader $(DEPS_vulkan-loader)
endif
endif

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_pkg,$(PLACEBO_URL),libplacebo)

.sum-libplacebo: $(PLACEBO_ARCHIVE)

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(APPLY) $(SRC)/libplacebo/0001-vulkan-meson-add-the-clang-gcc-C-runtime.patch
	$(APPLY) $(SRC)/libplacebo/0001-meson-allow-overriding-python3-path.patch
	$(MOVE)

.libplacebo: libplacebo crossfile.meson .python-venv
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) $(PLACEBOCONF)
	+$(MESONBUILD)
# Work-around for full paths to static libraries, which libtool does not like
# See https://github.com/mesonbuild/meson/issues/5479
	(cd $(UNPACK_DIR) && $(SRC_BUILT)/pkg-rewrite-absolute.py -i "$(PREFIX)/lib/pkgconfig/libplacebo.pc")
	touch $@
