SUBDIRS = btdht

bin_PROGRAMS = kadmin
kadmin_SOURCES = startup.cc bthread.cc bsocket.cc biothread.cc main.cc btcodec.cc bclock.cc butils.cc bclock.h biothread.h bsocket.h btcodec.h bthread.h butils.h bloader.cc bloader.h btimerd.cc btimerd.h bidle.cc bidle.h

kadmin_LDADD = \
	$(top_srcdir)/src/btdht/libsub_btdht.a \
	$(WS2_LDADD)

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir) -I$(top_srcdir)$(WS2_INCLUDE)
