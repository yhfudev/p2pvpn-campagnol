ACLOCAL_AMFLAGS = -I m4 --install
AUTOMAKE_OPTIONS = subdir-objects


# Misc and initialization
# -----------------------

EXTRA_DIST = Changelog
bin_PROGRAMS=
AM_CFLAGS = @COMMON_CFLAGS@


# Man pages
# ---------

EXTRA_DIST += man/campagnol.8.in man/campagnol.conf.5.in man/campagnol_rdv.8.in

man8_MANS =
if BUILD_CLIENT
man8_MANS += man/campagnol.8
man5_MANS = man/campagnol.conf.5
endif
if BUILD_SERVER
man8_MANS += man/campagnol_rdv.8
endif

man_do_subst = sed -e 's:@sysconfdir[@]:$(sysconfdir):g'\
	-e 's:@localstatedir[@]:$(localstatedir):g' \
	-e 's:@version[@]:$(PACKAGE_VERSION):g'

# Make sure that the man directory exists in the build tree
man/.tstamp:
	@$(MKDIR_P) man
	@: > $@
man/campagnol.8: man/.tstamp man/campagnol.8.in
	$(man_do_subst) < $(srcdir)/man/campagnol.8.in > man/campagnol.8
man/campagnol.conf.5: man/.tstamp man/campagnol.conf.5.in
	$(man_do_subst) < $(srcdir)/man/campagnol.conf.5.in > man/campagnol.conf.5
man/campagnol_rdv.8: man/.tstamp man/campagnol_rdv.8.in
	$(man_do_subst) < $(srcdir)/man/campagnol_rdv.8.in > man/campagnol_rdv.8

CLEANFILES = man/campagnol.8 man/campagnol.conf.5 man/campagnol_rdv.8
DISTCLEANFILES = man/.tstamp


# texinfo
# -------

#MAKEINFOFLAGS = --no-split
info_TEXINFOS = doc/campagnol.texi


# common/libcommon.a
# ------------------

EXTRA_DIST += lib/tdestroy.h \
	lib/cygwin_byteorder.h

noinst_LIBRARIES = common/libcommon.a
common_libcommon_a_SOURCES = common/bss_fifo.c common/bss_fifo.h \
	common/config_parser.c common/config_parser.h \
	common/config_io.c \
	common/strlib.c common/strlib.h \
	common/log.c common/log.h\
	common/pthread_wrap.h common/sem_wrap.h
common_libcommon_a_LIBADD = $(LIBOBJS)
common_libcommon_a_CPPFLAGS = @OPENSSL_CFLAGS@


# campagnol
# ---------

if BUILD_CLIENT
dist_sysconf_DATA = client/campagnol.conf

bin_PROGRAMS += campagnol
campagnol_SOURCES = client/bf_rate_limiter.c client/bf_rate_limiter.h \
	client/campagnol.c client/campagnol.h \
	client/communication.c client/communication.h \
	client/configuration.c client/configuration.h \
	client/dtls_utils.c client/dtls_utils.h \
	client/net_socket.c client/net_socket.h \
	client/peer.c client/peer.h \
	client/rate_limiter.c client/rate_limiter.h \
	client/tun_device_common.c client/tun_device.h
if HAVE_LINUX
campagnol_SOURCES += client/tun_device_linux.c
endif
if HAVE_FREEBSD
campagnol_SOURCES += client/tun_device_freebsd.c
endif
if HAVE_OPENBSD
campagnol_SOURCES += client/tun_device_openbsd.c
endif
if HAVE_CYGWIN
campagnol_SOURCES += client/tun_device_cygwin.c client/tap-win32_common.h
endif
if HAVE_NETBSD
campagnol_SOURCES += client/tun_device_netbsd.c
endif

campagnol_CPPFLAGS = -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\"" @OPENSSL_CFLAGS@
campagnol_CFLAGS = @COMMON_CFLAGS@
campagnol_LDADD = common/libcommon.a -lm @OPENSSL_LIBS@ @CLIENT_LIBS@
endif


# samples/ installation
# ---------------------

if BUILD_CLIENT
nobase_dist_pkgdata_DATA = \
	samples/sample_certs/cert1/key.pem \
	samples/sample_certs/cert1/certificate.pem \
	samples/sample_certs/cert2/key.pem \
	samples/sample_certs/cert2/certificate.pem \
	samples/sample_certs/cert_revoked/key.pem \
	samples/sample_certs/cert_revoked/certificate.pem \
	samples/sample_certs/cacert.pem \
	samples/sample_certs/crl.pem \
	samples/sample_certs/client1.conf \
	samples/sample_certs/client2.conf \
	samples/sample_certs/README \
	samples/ca_wrap.sh samples/README.ca_wrap
endif


# campagnol_rdv
# -------------

if BUILD_SERVER
bin_PROGRAMS += campagnol_rdv
campagnol_rdv_SOURCES = rdvserver/campagnol_rdv.c rdvserver/rdv.h \
	rdvserver/net_socket.c rdvserver/net_socket.h \
	rdvserver/peer.c rdvserver/peer.h \
	rdvserver/session.c rdvserver/session.h \
	rdvserver/server.c rdvserver/server.h

campagnol_rdv_CPPFLAGS = -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\""
campagnol_rdv_CFLAGS = @COMMON_CFLAGS@
campagnol_rdv_LDADD = common/libcommon.a @RDV_LIBS@
endif


# contrib
# -------

EXTRA_DIST += contrib/gentoo/campagnol-0.3.3.ebuild \
	contrib/gentoo/campagnol.conf contrib/gentoo/campagnol.init \
	contrib/gentoo/campagnol_rdv.conf contrib/gentoo/campagnol_rdv.init

