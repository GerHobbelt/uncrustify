#! /bin/sh
#
# Scans src/puntuators.cpp and creates punctuators.h
#

if [ ! -d src ] ; then
  cd ..
fi

python scripts/punc.py > src/punctuators.h

