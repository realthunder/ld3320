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
    path="works/arduino/code/$base"
    rsync='rsync -zavrl --partial --exclude=*.sw* --exclude=*build-* --progress --no-p --chmod=ugo=rwX' 
    for p in ../Arduino-Makefile ../libraries ../$base; do
        $rsync $p $remote:$path/../ || exit
    done
    ssh $remote "cd $path && make $@"
else
    make "$@"
fi
