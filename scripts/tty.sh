test "$TTYDEV" || TTYDEV=/dev/ttyACM0
# first lock for tty operation
TTYLOCK=/tmp/`basename $TTYDEV`.lock
# second lock for monitor stopping (end with hook stop)
TTYLOCK2=/tmp/`basename $TTYDEV`.lock2
# third lock for monitor killing (end with hook kill)
TTYLOCK3=/tmp/`basename $TTYDEV`.lock3

if [ -z "$TTYINCLUDED" ]; then
  exec 100> $TTYLOCK
  exec 101> $TTYLOCK2
  exec 102> $TTYLOCK3
  TTYINCLUDED=1
fi

tty_lock() {
  flock 100
}

tty_lock2() {
  flock 101
}

tty_unlock() {
  flock -u 100
}

tty_unlock2() {
  flock -u 101
}

tty_lock3() {
  flock 102
}

tty_trylock3() {
  flock -n 102
}

tty_unlock3() {
  flock -u 102
}

tty_read()  {
  (
    exec < $TTYDEV
    stty -echo igncr
    
    read line
    while read line; do
      [ "$line" = ">" ] && break
      echo $line
    done
  )
}

tty_send() {
  echo "$@" > $TTYDEV
}

tty_run() {
  tty_read &
  sleep 0.05 &> /dev/null || sleep 1
  tty_send "$@"
  wait
}
