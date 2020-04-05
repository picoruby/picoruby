#!/bin/sh

set -ex

TMPDIR=$(dirname $0)/tmp
OUT=$TMPDIR/socat.out

socat -d -d pty,raw,echo=0 pty,raw,echo=0 </dev/null > $OUT 2>&1 &

echo $! > $TMPDIR/pts.pid

sleep 1
head $OUT -n 1| awk '{print $7}' > $TMPDIR/pts0
sed '1d' $OUT | head -n 1 | awk '{print $7}' > $TMPDIR/pts1
rm $OUT
