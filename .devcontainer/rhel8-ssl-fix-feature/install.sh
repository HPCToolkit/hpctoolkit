#!/bin/sh

# Later commands use the nanolayer CLI, which doesn't understand the SSL certificates
# present in RHEL 8. Setting this environment variable helps.
# Why? I don't get it either. But this seems to work.

cd /etc/ssl/certs || exit $?
awk 'BEGIN {c=0;} /BEGIN CERT/{c++} { print > "cert." c ".crt"}' < ca-bundle.crt
for file in cert.*.crt; do ln -s "$file" "$(openssl x509 -hash -noout -in "$file")".0; done
