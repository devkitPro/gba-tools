#!/bin/sh

touch AUTHORS ChangeLog NEWS README.md
aclocal
autoconf
automake --add-missing -c
