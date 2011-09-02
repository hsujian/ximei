PROGRAMS = rand_ab
TARGETS  = $(PROGRAMS)

PROGRAM_LDADD        = `apr-1-config --includes` `apr-1-config --link-ld --libs` 

rand_ab_OBJECTS = rand_ab.lo
rand_ab_LDADD = $(PROGRAM_LDADD) $(SSL_LIBS)
rand_ab: $(rand_ab_OBJECTS)
	$(LINK) $(ab_LTFLAGS) $(rand_ab_OBJECTS) $(rand_ab_LDADD)

