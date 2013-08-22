noinst_PROGRAMS =

AM_CFLAGS = \
	$(COGL_CFLAGS)			\
	$(GLIB_CFLAGS)			\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/pe		\
	-Wall				\
	-Wextra				\
	-Wstrict-prototypes		\
	-Wno-unused-parameter		\
	$(NULL)

LDADD = 				\
	$(top_srcdir)/pe/.libs/libpe.la \
	$(COGL_LIBS) 			\
	$(GLIB_LIBS)			\
	-lm				\
	$(NULL)

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
