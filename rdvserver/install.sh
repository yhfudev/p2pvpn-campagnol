#! /bin/sh

die() {
	echo $1
	exit 1
}

usage="\
Build and install the RDV server

Usage: $0 bindir pkgdatadir

  bindir: The directory for installing executable (eg. /usr/local/bin)
  pkgdatadir: The directory for installing the jar archive (eg. /usr/local/share/campagnol)
"

if [ $# -lt 2 ]; then
	echo "$usage"
	exit 1
fi

echo "* Build server"
ant jar || die "Could not build the RDV server"

bindir=$1
pkgdatadir=$2
echo "* mkdir $bindir"
[ -d "$bindir" ] || mkdir -p "$bindir" || die "Could not create $bindir"
echo "* mkdir $pkgdatadir"
[ -d "$pkgdatadir" ] || mkdir -p "$pkgdatadir" || die "Could not create $pkgdatadir"

# Be sure that the paths are absolute
bindir=$(cd "$bindir" && pwd)
pkgdatadir=$(cd "$pkgdatadir" && pwd)
destjar="${pkgdatadir}/campagnol_rdv.jar"
destbin="${bindir}/campagnol_rdv"

shellscript="\
#! /bin/sh
exec java -jar \"${destjar}\" \"\${@}\""
echo "$shellscript" > campagnol_rdv
chmod +x ./campagnol_rdv

echo "* Install ${destjar}"
install -m 0644 ./campagnol_rdv.jar "$pkgdatadir" || die "Error while installing into $pkgdatadir"
echo "* Install ${destbin}"
install -m 0755 ./campagnol_rdv "$bindir" || die "Error while installing into $bindir"
