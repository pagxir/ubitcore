SUBDIRS = btdht

bin_PROGRAMS = kadmin
kadmin_SOURCES = startup.cc bthread.cc bsocket.cc biothread.cc main.cc btcodec.cc sha1.cc bclock.cc butils.cc bclock.h biothread.h bsocket.h btcodec.h bthread.h butils.h sha1.h bloader.cc bloader.h btimerd.cc btimerd.h bidle.cc bidle.h

noinst_LIBRARIES = libbtdht.a
libbtdht_a_SOURCES = startup.cc bthread.cc bsocket.cc biothread.cc main.cc btcodec.cc sha1.cc bclock.cc butils.cc bclock.h biothread.h bsocket.h btcodec.h bthread.h butils.h sha1.h dhtloader.cc bloader.h btimerd.cc btimerd.h bidle.cc bidle.h

libbtdht_a_LIBADD = \
	$(top_srcdir)/src/btdht/libsub_btdht.a \
	$(WS2_LDADD)

kadmin_LDADD = \
	$(top_srcdir)/src/btdht/libsub_btdht.a \
	$(WS2_LDADD)

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir) -I$(top_srcdir)$(WS2_INCLUDE)
