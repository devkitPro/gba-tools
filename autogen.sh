#!/bin/sh

touch AUTHORS ChangeLog NEWS README
aclocal
autoconf
automake --add-missing -c
