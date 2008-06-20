#! /bin/sh
# Keyboard startup/shutdown script
CONFIGURED=$HOME/.osso/hildon-input-method/.configured

if [ "x$AF_PIDDIR" = "x" ]; then
  echo "$0: Error, AF_PIDDIR is not defined"
  exit 2
fi
if [ ! -w $AF_PIDDIR ]; then
  echo "$0: Error, directory $AF_PIDDIR is not writable"
  exit 2
fi
# Small hack to have it working NOW
if [ "x$LAUNCHWRAPPER_NICE_TRYRESTART" = "x" ]; then
  echo "$0: LAUNCHWRAPPER_NICE_TRYRESTART is not defined - starting with LAUNCHWRAPPER"
  LAUNCHWRAPPER_NICE_TRYRESTART=$LAUNCHWRAPPER
fi

if [ ! -f $CONFIGURED ]; then 
  mkdir -p $HOME/.osso/hildon-input-method
  if [ -x /usr/bin/hildon-input-method-configurator ]; then
    /usr/bin/hildon-input-method-configurator && touch $CONFIGURED
  fi
fi

PROG=/usr/bin/hildon-input-method
SVC="Keyboard"

case "$1" in
start)  START=TRUE
        ;;
stop)   START=FALSE
        ;;
*)      echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac

if [ $START = TRUE ]; then
  # check that required environment is defined
  if [ "x$DISPLAY" = "x" ]; then
    echo "$0: Error, DISPLAY is not defined"
    exit 2
  fi
   source $LAUNCHWRAPPER_NICE_TRYRESTART start "$SVC" "$PROG"
else
	 source $LAUNCHWRAPPER_NICE_TRYRESTART stop "$SVC" "$PROG"
fi
