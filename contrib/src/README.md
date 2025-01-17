## Writing rules

At the bare minimum, a package in contrib must provide two Makefile
targets in `src/foo/rules.mak`:
 - `.foo` to build and install the package, and
 - `.sum-foo` to fetch or create a source tarball and verify it,
where foo the package name.


### Tarball

`.sum-foo` typically depends on a separate target that fetches the source
code. In that case, `.sum-foo` needs only verify that the tarball
is correct, e.g.:


	$(TARBALLS)/libfoo-$(FOO_VERSION).tar.bz2:
		$(call download,$(FOO_URL))

	# This will use the default rule: check SHA-512
	.sum-foo: libfoo-$(FOO_VERSION).tar.bz2

NOTE: contrary to the previous VLC contribs, this system always uses
a source tarball, even if the source code is downloaded from a VCS.
This serves two purposes:
 - offline builds (or behind a firewall),
 - source code requirements compliance.


### Compilation

Similarly, `.foo` typically depends on the source code directory. In this
case, care must be taken that the directory name only exists if the
source code is fully ready. Otherwise Makefile dependencies will break
(this is not an issue for files, only directories).

	libfoo: libfoo-$(FOO_VERSION).tar.bz2 .sum-foo
		$(UNPACK) # to libfoo-$(FOO_VERSION)
		### apply patches here ###
		# last command: make the target directory
		$(MOVE)

	.foo: libfoo
		$(MAKEBUILDDIR)
		$(MAKECONFIGURE)
		+$(MAKEBUILD)
		+$(MAKEBUILD) install
		touch $@

### Conditional builds

As far as possible, build rules should determine automatically whether
a package is useful (for VLC media player) or not. Useful packages
should be listed in the PKGS special variable. See some examples:

	# FFmpeg is always useful
	PKGS += ffmpeg

	# DirectX headers are useful only on Windows
	ifdef HAVE_WIN32
	PKGS += directx
	endif

	# x264 is only useful when stream output is enabled
	ifdef BUILD_ENCODERS
	PKGS += x264
	endif

If a package is a dependency of another package, but it is not a
direct dependency of VLC, then it should NOT be added to PKGS. The
build system will automatically build it via dependencies (see below).

Some packages may be provided by the target system. This is especially
common when building natively on Linux or BSD. When this situation is
detected, the package name should be added to the PKGS_FOUND special
variable. The build system will then skip building this package:

	# Asks pkg-config if foo version 1.2.3 or later is present:
	ifeq ($(call need_pkg,'foo >= 1.2.3'),)
	PKGS_FOUND += foo
	endif


### Dependencies

If package bar depends on package foo, the special `DEPS_bar` variable
should be defined as follow:

	DEPS_bar = foo $(DEPS_foo)

Note that dependency resolution is unfortunately _not_ recursive.
Therefore `$(DEPS_foo)` really should be specified explicitly as shown
above. (In practice, this will not make any difference insofar as there
are no pure second-level nested dependencies. For instance, libass
depends on FontConfig, which depends on FreeType, but libass depends
directly on FreeType anyway.)

Also note that `DEPS_bar` is set "recursively" with `=`, rather than
"immediately" with `:=`. This is so that `$(DEPS_foo)` is expanded
correctly, even if `DEPS_foo` it is defined after `DEPS_bar`.

Implementation note:

	If you must know, the main.mak build hackery will automatically
	emit a dependency from .bar onto .dep-foo:

		.bar: .dep-foo

	...whereby .dep-foo will depend on .foo:

		.dep-foo: .foo
			touch $@

	...unless foo was detected in the target distribution:

		.dep-foo:
			touch $@

	So you really only need to set DEPS_bar.
