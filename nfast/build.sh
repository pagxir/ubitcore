#!/bin/sh
# $Id:$

export SRCDIR=`pwd`

cd ~/objs && make -f $SRCDIR/Makefile VPATH=$SRCDIR $*
