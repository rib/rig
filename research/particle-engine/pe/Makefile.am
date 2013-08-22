AM_CFLAGS = \
	$(COGL_CFLAGS)			\
	$(GLIB_CFLAGS)			\
	-I$(top_srcdir)			\
	-Wall				\
	-Wextra				\
	-Wstrict-prototypes		\
	-O0 -g				\
	$(NULL)

LDADD = $(COGL_LIBS) $(GLIB_LIBS) -lm

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
