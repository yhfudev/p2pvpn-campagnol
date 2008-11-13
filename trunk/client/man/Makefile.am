EXTRA_DIST = campagnol.8.in
dist_man_MANS = campagnol.8

do_subst = sed -e 's:@sysconfdir[@]:$(abspath $(sysconfdir)):g'\
	-e 's:@localstatedir[@]:$(abspath $(localstatedir)):g'

campagnol.8: campagnol.8.in
	$(do_subst) < campagnol.8.in > campagnol.8

CLEANFILES = campagnol.8
