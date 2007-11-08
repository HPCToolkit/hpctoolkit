TESTS="basic dlopen exit _exit _Exit nonzeroexit real_exit abort sigint fork exec forkexec pthreads pthreadsexit omp testsh testcsh testexec ThreadLister threads10"
#badpthreads badpthreadscancel badpthreadsasynccancel 
LIBMONITOR="$PWD/../libmonitor.so"
VERBOSE=0

debug() {
    if [ $VERBOSE -ne 0 ]; then
	echo $1;
    fi
}

do_test_loop() {
    export MONITOR_DEBUG=1
    OLD=$LD_PRELOAD
    for i in $TESTS; 
      do
      if [ -x ./$i ]; then
	  debug "./$i a=b c=d -X > $i.$1.out 2>&1";
	  export LD_PRELOAD=$LIBTESTTOOL:$LIBMONITOR:$OLD
	  ./$i a=b c=d -X > $i.$1.out 2>&1 
	  export LD_PRELOAD=$OLD
	  if [ -e ./$i.$1.regex ]; then
	      debug "./verify $i.$1"
	      ./verify $i.$1
	      debug ""
	  else
	      debug "$i.$1 has (no test case)"
	      echo "$i.$1: SKIPPED";
	  fi
      elif [ -e "./$i.class" ]; then
	  debug "java ./$i a=b c=d -X > $i.$1.out 2>&1";
	  export LD_PRELOAD=$LIBTESTTOOL:$LIBMONITOR:$OLD
	  java ./$i a=b c=d -X < /dev/null > $i.$1.out 2>&1 
	  export LD_PRELOAD=$OLD
	  if [ -e ./$i.$1.regex ]; then
	      debug "./verify $i.$1"
	      ./verify $i.$1
	      debug ""
	  else
	      debug "$i.$1 has (no test case)"
	      echo "$i.$1: SKIPPED";
	  fi
      else
	  debug "$i.$1 does not exist"
          echo "$i.$1: SKIPPED";
      fi
    done
    export LD_PRELOAD=$OLD
}

show_help() {
	echo "Usage: $0 [-hv] [tests]";
	echo " -h Print this message."
	echo " -v Verbose output."
	echo "";
	echo " [tests] if specified, can be one or more of:";
	echo "$TESTS";
}

while getopts hv OPT
do
    case ${OPT} in
    h)    show_help;
          exit 0;;
    v)    VERBOSE=1; shift 1;;
    \?)   show_help;
          exit 1;;
    esac
done

if [ ! -z "$*" ]; then
    TESTS=$*;
fi
debug "Doing tests: $TESTS";
debug "";

if [ ! -e $LIBMONITOR ]; then
    echo "$LIBMONITOR not found.";
    exit 1;
fi

LIBTESTTOOL="$PWD/libtesttool-0.so"
debug "Testing with only library and process callbacks: $LIBTESTTOOL"
debug ""

if [ ! -e $LIBTESTTOOL ]; then
    echo "$LIBTESTTOOL not found.";
    exit 1;
fi

do_test_loop 0;

LIBTESTTOOL="$PWD/libtesttool-1.so"
debug "Testing with full set of callbacks, including abnormal termination: $LIBTESTTOOL"
debug ""

if [ ! -e $LIBTESTTOOL ]; then
    echo "$LIBTESTTOOL not found.";
    exit 1;
fi

do_test_loop 1;

debug ""
debug "All tests finished."

unset MONITOR_DEBUG
