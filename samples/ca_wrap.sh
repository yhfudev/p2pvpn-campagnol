#!/bin/sh

# Copyright (C) 2009 Florent Bondoux
#
# This file is part of Campagnol.
#
# Campagnol is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Campagnol is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Campagnol.  If not, see <http://www.gnu.org/licenses/>.



# Working directory
TOP_DIR="./CA"

# Default values for the X509 certificates
SSL_DAYS_DEFAULT="730"
SSL_BITS_DEFAULT="1024"
SSL_COUNTRY_DEFAULT="FR"
SSL_STATE_DEFAULT="France"
SSL_LOCALITY_DEFAULT="Paris"
SSL_ORGANIZATION_DEFAULT="Campagnol VPN"
SSL_UNIT_DEFAULT="For testing purpose only"
SSL_COMMONNAME_DEFAULT=`hostname -f`
SSL_EMAIL_DEFAULT="root@${SSL_COMMONNAME_DEFAULT}"
SSL_CRL_DAYS_DEFAULT="30"


# files and directories relative to TOP_DIR
## OpenSSL config file
SSL_CONF="ssl.cnf"
## next serial number
CA_SERIAL="serial"
## CA databae
CA_INDEX="index.txt"
## CA root certificate
CA_CERTIFICATE="cacert.pem"
## CA private key
CA_PRIV_KEY="private/cakey.pem"
## output directory for the certificates
CA_CERTS="certs"
## certificate revocation list
CA_CRL="crl.pem"

################################################################################

OPENSSL="openssl"

CWD=`pwd`
# create TOP_DIR
mkdir -p "${TOP_DIR}" || exit 1
chmod go-rwx "${TOP_DIR}" || exit 1
cd "${TOP_DIR}"
# and get absolute path name
ABS_TOP_DIR=`pwd`
cd ${CWD}

# check whether SSL_CONF exists
check_cnf() {
	if [ ! -e "${ABS_TOP_DIR}/${SSL_CONF}" ]; then
		return 1
	fi
	return 0
}

# chech whether CA_CERTIFICATE exists
check_ca() {
	if [ ! -e "${ABS_TOP_DIR}/${CA_CERTIFICATE}" ]; then
		return 1
	fi
	if [ ! -e "${ABS_TOP_DIR}/${CA_PRIV_KEY}" ]; then
		return 1
	fi
	if [ ! -e "${ABS_TOP_DIR}/${CA_SERIAL}" ]; then
		return 1
	fi
	if [ ! -e "${ABS_TOP_DIR}/${CA_INDEX}" ]; then
		return 1
	fi
	return 0;
}

check_all() {
	check_cnf || { echo "!! Configuration file is missing" && return 1; }
	check_ca || { echo "!! some CA files are missing" && return 1; }
	return 0
}

# create_dir filename
# create the base directory of filename
create_subdir() {
	local directory=`dirname "${ABS_TOP_DIR}/${1}"`
	mkdir -p "${directory}"
	return $?
}

create_dirs() {
	echo "* Creating subdirectories"

	create_subdir "${SSL_CONF}" || return $?
	create_subdir "${CA_SERIAL}" || return $?
	create_subdir "${CA_INDEX}" || return $?
	create_subdir "${CA_CERTIFICATE}" || return $?
	create_subdir "${CA_PRIV_KEY}" || return $?
	mkdir -p "${ABS_TOP_DIR}/${CA_CERTS}" || return $?
	
	return $?
}

# Create the base directories and the config file
create_cnf() {
	create_dirs || return $?
	
	echo "* Generating OpenSSL configuration files"
	
	if [ -e "${ABS_TOP_DIR}/${SSL_CONF}" ]; then
		echo "!! ${ABS_TOP_DIR}/${SSL_CONF} exists already"
		return 1
	fi

	# local variable, defined from the environnemnet or *_DEFAULT variables
	local ssl_days="${SSL_DAYS:-${SSL_DAYS_DEFAULT}}"
	local ssl_bits="${SSL_BITS:-${SSL_BITS_DEFAULT}}"
	local ssl_country="${SSL_COUNTRY:-${SSL_COUNTRY_DEFAULT}}"
	local ssl_state="${SSL_STATE:-${SSL_STATE_DEFAULT}}"
	local ssl_locality="${SSL_LOCALITY:-${SSL_LOCALITY_DEFAULT}}"
	local ssl_organization="${SSL_ORGANIZATION:-${SSL_ORGANIZATION_DEFAULT}}"
	local ssl_unit="${SSL_UNIT:-${SSL_UNIT_DEFAULT}}"
	local ssl_commonname="${SSL_COMMONNAME:-${SSL_COMMONNAME_DEFAULT}}"
	local ssl_email="${SSL_EMAIL:-${SSL_EMAIL_DEFAULT}}"
	local ssl_crl_days="${SSL_CRL_DAYS:-${SSL_CRL_DAYS_DEFAULT}}"
	
	# config file
	cat <<-EOF > "${ABS_TOP_DIR}/${SSL_CONF}"
		[ ca ]
		default_ca         = CA_default        # The default ca section
		
		[ CA_default ]
		dir                = ${ABS_TOP_DIR}    # working directory
		database           = \$dir/${CA_INDEX}       # database index file
		new_certs_dir      = \$dir/${CA_CERTS}       # default place for new certs.
		
		certificate        = \$dir/${CA_CERTIFICATE} # The CA certificate
		serial             = \$dir/${CA_SERIAL}      # The current serial number
		private_key        = \$dir/${CA_PRIV_KEY}    # The private key
		
		name_opt           = ca_default        # SUbject Name options
		cert_opt           = ca_default        # Certificate field options
		
		default_days       = ${ssl_days}       # how long to certify for
		default_crl_days   = ${ssl_crl_days}   # How long before next CRL
		default_md         = sha1              # which digest to use
		preserve           = no                # keep passed DN ordering
		
		# A few difference way of specifying how similar the request should look
		# For type CA, the listed attributes must be the same, and the optional
		# and supplied fields are just that :-)
		policy                  = policy_match
		
		# For the CA policy
		[ policy_match ]
		countryName             = match
		stateOrProvinceName     = match
		organizationName        = match
		organizationalUnitName  = optional
		commonName              = supplied
		emailAddress            = optional
		
		[ req ]
		default_bits            = ${ssl_bits}
		distinguished_name      = req_dn
		
		[ req_dn ]
		countryName                     = Country Name (2 letter code)
		countryName_min                 = 2
		countryName_max                 = 2
		countryName_default             = ${ssl_country}
		stateOrProvinceName             = State or Province Name (full name)
		stateOrProvinceName_default     = ${ssl_state}
		localityName                    = Locality Name (eg, city)
		localityName_default            = ${ssl_locality}
		organizationName                = Organization Name (eg, company)
		organizationName_default        = ${ssl_organization}
		organizationalUnitName          = Organizational Unit Name (eg, section)
		organizationalUnitName_default  = ${ssl_unit}
		commonName                      = Common Name (eg, YOUR name)
		commonName_default              = ${ssl_commonname}
		emailAddress                    = Email Address
		emailAddress_default            = ${ssl_email}
		
		[ v3_ca ]
		basicConstraints        = CA:true
		subjectKeyIdentifier    = hash
		authorityKeyIdentifier  = keyid:always,issuer:always
	EOF
	
	echo "* Configuration file: ${ABS_TOP_DIR}/${SSL_CONF}"
	
	return $?
}

# Create the CA key and self signed certificate
gen_ca() {
	check_cnf || create_cnf || return $?
	
	if check_ca; then
		echo "!! Some files are already present"
		return 1
	fi

	echo "* Generating the CA key and certificate"

	local ssl_days="${SSL_DAYS:-${SSL_DAYS_DEFAULT}}"

	${OPENSSL} req -new -x509 -days ${ssl_days} -extensions v3_ca \
    	-keyout "${ABS_TOP_DIR}/${CA_PRIV_KEY}" \
		-out "${ABS_TOP_DIR}/${CA_CERTIFICATE}" \
		-config "${ABS_TOP_DIR}/${SSL_CONF}" \
    	|| return $?

	chmod 0400 "${ABS_TOP_DIR}/${CA_PRIV_KEY}" || return $?
	chmod 0444 "${ABS_TOP_DIR}/${CA_CERTIFICATE}" || return $?
	
	# CA serial file
	echo "01" > "${ABS_TOP_DIR}/${CA_SERIAL}" || return $?
	
	# empty CA database
	echo -n "" > "${ABS_TOP_DIR}/${CA_INDEX}" || return $?
	
	echo "* CA private key: ${ABS_TOP_DIR}/${CA_PRIV_KEY}"
	echo "* CA root certificate: ${ABS_TOP_DIR}/${CA_CERTIFICATE}"
	
	return $?
}

# gen_cert <dirname>
# Create a key and a signed certificate in 'dirname'
gen_cert() {
	check_all || return $?

	if [ -n "${1}" ]; then
		echo "* Generating a client key and certificate request in ${1}"
	
		local cl_dir="${ABS_TOP_DIR}/${1}"
		local req_file="${cl_dir}/${$}_${1}.csr"
		
		mkdir -p "${cl_dir}" || return $?
		
		# use $1 for the default organizationalUnitName
		sed -i -e \
"s:organizationalUnitName_default.*:organizationalUnitName_default=${1}:" \
"${ABS_TOP_DIR}/${SSL_CONF}"
		${OPENSSL} req -new -nodes -keyout "${cl_dir}/key.pem" \
		-out "${req_file}" -config "${ABS_TOP_DIR}/${SSL_CONF}" || return $?

		chmod 0400 "${cl_dir}/key.pem" || return $?

		echo "* Signing the client certificate"
		
		${OPENSSL} ca -config "${ABS_TOP_DIR}/${SSL_CONF}" -in "${req_file}" \
		-out "${cl_dir}/certificate.pem" || return $?

		chmod 0444 "${cl_dir}/certificate.pem" || return $?
		
		rm -f ${req_file}
		
		echo "* CA certificate: ${ABS_TOP_DIR}/${CA_CERTIFICATE}"
		echo "* key: ${cl_dir}/key.pem"
		echo "* certificate: ${cl_dir}/certificate.pem"
	else
		return 1
	fi
	
	return $?
}

# Create a CRL into CA_CRL
gen_crl() {
	check_all || return $?
	${OPENSSL} ca -config "${ABS_TOP_DIR}/${SSL_CONF}" -gencrl \
		-out "${ABS_TOP_DIR}/${CA_CRL}" || return $?
	
	echo "* CRL: ${ABS_TOP_DIR}/${CA_CRL}"
	
	return $?
}

# Show the current CRL
print_crl() {
	check_all || return $?
	${OPENSSL} crl -text -noout -in "${ABS_TOP_DIR}/${CA_CRL}"
	
	return $?
}

# revoke_crt <dirname>
# Revoke the certificate in $1/certificate.pem
revoke_crt() {
	check_all || return $?
	echo "* Revoking the certificate in ${1}/certificate.pem"
	
	${OPENSSL} ca -revoke "${ABS_TOP_DIR}/$1/certificate.pem" \
		-config "${ABS_TOP_DIR}/${SSL_CONF}"
	
	return $?
}

# print the list of the generated certificates
print_certs() {
	check_all || return $?
	for crt in "${ABS_TOP_DIR}/${CA_CERTS}/"*.pem ; do
		echo "* ${TOP_DIR}/${CA_CERTS}/$(basename ${crt})"
		${OPENSSL} x509 -serial -subject -dates -fingerprint -noout -in "$crt"
		echo
	done
}

print_help() {
	echo \
"Usage: ca_wrap.sh command [arg]...

Commands:
  * gen_conf                create the basic CA directories and files
  * gen_ca                  generate the CA key and certificate
  * gen_cert <dirname>      generate a new client key and signed certificate
                            into \"dirname\"
  * gen_crl                 generate a CRL into \"crl.pem\"
  * print_certs             print out the generated certificates
  * print_crl               print out the CRL
  * revoke_crt <dirname>    revoke the certificate stored in \"dirname\"
"
}

case ${1} in
	gen_conf)
		create_cnf
		;;
	gen_ca)
		gen_ca
		;;
	gen_cert)
		if [ -n "${2}" ]; then
			gen_cert ${2}
		else
			print_help
		fi
		;;
	gen_crl)
		gen_crl
		;;
	print_certs)
		print_certs
		;;
	print_crl)
		print_crl
		;;
	revoke_crt)
		if [ -n "${2}" ]; then
			revoke_crt ${2}
		else
			print_help
		fi
		;;
	*)
		print_help
		;;
esac
