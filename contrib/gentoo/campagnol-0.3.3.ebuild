# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=2

DESCRIPTION="Campagnol is a decentralized VPN over UDP tunneling"
HOMEPAGE="http://campagnol.sourceforge.net"
SRC_URI="http://downloads.sourceforge.net/campagnol/${P}.tar.bz2"
RESTRICT="mirror"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE="+client doc server"

COMMON_DEPEND=">=dev-libs/openssl-0.9.8j"
DEPEND="${COMMON_DEPEND}
	doc? ( sys-apps/texinfo )"
RDEPEND="${COMMON_DEPEND}"

src_configure() {
	# Force localstatedir to fix the default pidfile path
	econf --localstatedir=/var \
		--docdir="/usr/share/doc/${PF}" \
		$(use_enable client) \
		$(use_enable server) || die "configure failed"
}

src_compile() {
	emake || die "compile failed"
	if use doc; then
		emake html || die "emake html failed"
	fi
}

src_install() {
	make DESTDIR="${D}" install || die "Installation failed"

	if use client; then
		# copy the sample configuration file
		dodir /usr/share/${PN}
		insinto /usr/share/${PN}
		doins client/campagnol.conf || die
		newinitd "${FILESDIR}/${PN}.init" campagnol || die
		newconfd "${FILESDIR}/${PN}.conf" campagnol || die
	fi

	if use server; then
		newinitd "${FILESDIR}/${PN}_rdv.init" campagnol_rdv || die
		newconfd "${FILESDIR}/${PN}_rdv.conf" campagnol_rdv || die
	fi

	dodoc README Changelog TODO || die
	if use doc; then
		dohtml doc/campagnol.html/*.html || die
	fi
}

pkg_postinst() {
	elog "You will need the Universal TUN/TAP driver compiled into"
	elog "your kernel or as a module to use ${PN}."
	if use client; then
		elog ""
		elog "The /usr/share/${PN}/samples directory contains some samples"
		elog "certificates and a helper script to create and manage a small CA."
	fi
}
