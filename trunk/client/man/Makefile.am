EXTRA_DIST = campagnol.8.in campagnol.conf.5.in
dist_man_MANS = campagnol.8 campagnol.conf.5

do_subst = sed -e 's:@sysconfdir[@]:$(abspath $(sysconfdir)):g'\
	-e 's:@localstatedir[@]:$(abspath $(localstatedir)):g' \
	-e 's:@version[@]:$(PACKAGE_VERSION):g'

campagnol.8: campagnol.8.in
	$(do_subst) < campagnol.8.in > campagnol.8
campagnol.conf.5: campagnol.conf.5.in
	$(do_subst) < campagnol.conf.5.in > campagnol.conf.5

CLEANFILES = campagnol.8 campagnol.conf.5
