AM_CFLAGS = \
	$(CG_DEP_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib

LDADD = \
	$(CG_DEP_LIBS) \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(top_builddir)/clib/clib/libclib.la \
	-lm

particle_engine_sources = fuzzy.c particle-engine.c
particle_emitter_sources = particle-emitter.c
particle_system_sources = particle-system.c
particle_swarm_sources = particle-swarm.c

lib_LTLIBRARIES = libpe.la
libpe_la_SOURCES = \
	$(particle_engine_sources)	\
	$(particle_emitter_sources)	\
	$(particle_system_sources)	\
	$(particle_swarm_sources)	\
	$(NULL)
