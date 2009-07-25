AUTOMAKE_OPTIONS = subdir-objects

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = bss_fifo.c bss_fifo.h \
	config_parser.c config_parser.h \
	log.c log.h\
	pthread_wrap.h sem_wrap.h
libcommon_a_LIBADD = $(LIBOBJS)
AM_CFLAGS = @COMMON_CFLAGS@
libcommon_a_CPPFLAGS = @OPENSSL_CFLAGS@
