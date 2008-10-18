SUBDIRS = btdht
bin_PROGRAMS = btload
btload_SOURCES = startup.cc bthread.cc bsocket.cc biothread.cc burlget.cc main.cc boutfile.cc btcodec.cc sha1.cc bpeermgr.cc btracker.cc bqueue.cc bpeer.cc bclock.cc butils.cc boffer.cc bupdown.cc bchunk.cc bitfield.cc bfiled.cc bchunk.h bclock.h bfiled.h biothread.h bitfield.h boffer.h boutfile.h bpeer.h bpeermgr.h bqueue.h bsocket.h btcodec.h bthread.h btracker.h bupdown.h burlget.h butils.h sha1.h

btload_LDADD = \
	$(top_srcdir)/src/btdht/libsub_btdht.a \
	$(WS2_LDADD)

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir) -I$(top_srcdir)$(WS2_INCLUDE)
