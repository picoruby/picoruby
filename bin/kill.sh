#!/bin/sh

set -ex

TMPDIR=$(dirname $0)/tmp

rm -f $TMPDIR/pts0
rm -f $TMPDIR/pts1
cat $TMPDIR/pts.pid | xargs kill -9
rm $TMPDIR/pts.pid
