#! /bin/sh
# Keyboard startup/shutdown script
CONFIGURED=/etc/hildon-input-method.configured

if [ "x$AF_PIDDIR" = "x" ]; then
  echo "$0: Error, AF_PIDDIR is not defined"
  exit 2
fi
if [ ! -w $AF_PIDDIR ]; then
  echo "$0: Error, directory $AF_PIDDIR is not writable"
  exit 2
fi

# Small hack to have it working NOW
if [ "x$LAUNCHWRAPPER" = "x" ]; then
  LAUNCHWRAPPER=/etc/osso-af-init/launch-wrapper.sh
fi
if [ "x$LAUNCHWRAPPER_NICE_TRYRESTART" = "x" ]; then
  echo "$0: LAUNCHWRAPPER_NICE_TRYRESTART is not defined - starting with LAUNCHWRAPPER"
  LAUNCHWRAPPER_NICE_TRYRESTART=$LAUNCHWRAPPER
fi

if [ ! -f $CONFIGURED ]; then 
  if [ -x /usr/bin/hildon-input-method-configurator ]; then
    if test "x$_SBOX_DIR" = "x";then
      /usr/bin/hildon-input-method-configurator && sudo /bin/touch $CONFIGURED
    else
      /usr/bin/hildon-input-method-configurator && fakeroot /bin/touch $CONFIGURED
    fi
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
