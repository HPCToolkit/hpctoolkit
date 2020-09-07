#/bin/sh

cp "$1" "$2"
chmod u=rwx,g=rx,o=rx "$2"
