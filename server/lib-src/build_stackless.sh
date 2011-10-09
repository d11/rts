#!/bin/sh
INSTALLPATH=`pwd`/lib
cd python-3.2-stackless
./configure --prefix $INSTALLPATH --exec-prefix $INSTALLPATH
make
make install
