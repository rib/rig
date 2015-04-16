AM_CFLAGS = \
	$(COGL_DEP_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src

LDADD = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl2.la \
	$(top_builddir)/clib/src/libclib.la \
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
