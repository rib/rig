SUBDIRS = common i18n stubdata

if IO
SUBDIRS += io
endif

if TOOLS
SUBDIRS += tools
endif

if DATA
SUBDIRS += data
endif

if EXTRAS
SUBDIRS += extra
endif

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = icu-io.pc icu-uc.pc icu-i18n.pc
