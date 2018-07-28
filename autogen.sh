#!/bin/sh

touch AUTHORS ChangeLog NEWS
aclocal
autoconf
automake --add-missing -c
