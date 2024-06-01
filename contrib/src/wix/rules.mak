WIX_VERSION=314
WIX_FULL_VERSION=3.14.0.6526
WIX_URL := https://wixtoolset.org/downloads/v$(WIX_FULL_VERSION)/wix$(WIX_VERSION)-binaries.zip

ifdef HAVE_WIN32
# this requires dotnet 4.0 to be installed when running wix
PKGS += wix
# need to be installed when using prebuilt
PKGS_TOOLS += wix
endif

.sum-wix: wix$(WIX_FULL_VERSION).zip

$(TARBALLS)/wix$(WIX_FULL_VERSION).zip:
	$(call download_pkg,$(WIX_URL),wix)

wix: UNZIP_PARAMS=-d wix$(WIX_FULL_VERSION)
wix: wix$(WIX_FULL_VERSION).zip .sum-wix
	$(UNPACK)
	$(MOVE)

.wix: wix
	install -d "$(PREFIX)/bin"
	for f in $</*.exe $</*.dll ; do \
	  install $$f "$(PREFIX)/bin" ; \
	done
	touch $@
