#!/bin/sh
. ./tty.sh
tty_lock
tty_run "$@"
