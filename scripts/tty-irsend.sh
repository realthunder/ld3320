#!/bin/sh
[ ! -f $1 ] && echo invalid command file && exit 1
. tty.sh
tty_lock

ans=
irsend() {
  while read line; do
    [ -z "$line" ] && break
    ans=`tty_run "$line"`
    [ -z "$ans" ] && continue
    [ "$ans" != "${ans#Sent RAW}" ] && ans="set `basename $1`" && return 0
    return 1
  done < $1
  ans=`unexpected ending`
  return 1
}

irsend $1
ret=$?

echo $0 $@
[ $ret != 0 ] && echo $ans

log=/tmp/thlog/current.log
if test -f $log; then
  echo "`date +%T` $0 $@" >> $log
  [ $ret != 0 ] && echo $ret >> $log
fi

exit $ret
  
