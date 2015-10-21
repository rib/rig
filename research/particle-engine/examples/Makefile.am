noinst_PROGRAMS =

AM_CFLAGS = \
	$(CG_DEP_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib \
	-I$(top_srcdir)/research/particle-engine/pe

LDADD = \
	$(CG_DEP_LIBS) \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(top_builddir)/research/particle-engine/pe/libpe.la \
	$(top_builddir)/clib/clib/libclib.la \
	-lm

EXTRA_DIST = run-demos.sh

noinst_PROGRAMS += ants
ants_SOURCES = ants.c

noinst_PROGRAMS += catherine_wheel
catherine_wheel_SOURCES = catherine-wheel.c

noinst_PROGRAMS += fireworks
fireworks_SOURCES = fireworks.c

noinst_PROGRAMS += fish
fish_SOURCES = fish.c

noinst_PROGRAMS += fountains
fountains_SOURCES = fountains.c

noinst_PROGRAMS += galaxy
galaxy_SOURCES = galaxy.c

noinst_PROGRAMS += snow
snow_SOURCES = snow.c
