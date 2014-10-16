#!/bin/sh

remote=oneiric322
base=`basename $PWD`
board=
for arg in "$@"; do
    case $arg in
    board=*)
        board=${arg#board=}
        ;;
    esac
done

if test $board; then
    path="works/arduino/code"
    rsync='rsync -zarl --partial --exclude=*.sw* --exclude=*build-* --progress --no-p --chmod=ugo=rwX' 
    $rsync ../Teensy.mk ../Arduino-Makefile ../libraries ../$base $remote:$path || exit
    ssh $remote "cd $path/$base && make $@"
else
    make "$@"
fi
