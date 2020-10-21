#!/bin/sh
# vim:noet:ts=2:sw=2:et
# Fake the system time

dat=""
static=0
ld_preload_arg=""

usage()
{
  if [ "$1" ]; then
    exec >&2
    echo "$0: $1"
    echo
  fi
  echo "Usage: $0 [-s|--static] [-l|--add-ld-preload lib] date program args..."
  echo
  echo "Run 'program' with 'args'."
  echo "The program will believe that the current time is 'date'."
  echo
  [ "$1" ] && exit 1 || exit 0
}

while [ "$1" ] && [ -z "$dat" ]; do
  case "$1" in
    -s|--static)
      static=1
      ;;
    -l|--add-ld-preload)
      [ "$2" ] || usage "Missing argument for the '$1' option"
      ld_preload_arg="$2"
      shift;
      ;;
    -v|--version)
      echo "$0: Version @VERSION@"
      echo ""
      echo "For usage information, use '$0 --help'."
      exit 0
      ;;
    -h|-?|--help)
      usage
      ;;
    -*)
      usage "Invalid option '$1'"
      ;;
    *)
      [ "$2" ] || usage "Missing 'program' argument"
      dat="$1"
      ;;
  esac
  shift
done

[ "$dat" ] || usage "Missing 'date' argument"

# Assume that 'date' already printed an error message
sec1=$(date -d "$dat" '+%s')
[ $? -eq 0 ] || exit 1

sec2=$(expr $(date '+%s') - $sec1)
# According to its documentation expr returns exit status 1,
# when the expression evaluates to 0
[ $? -le 1 ] || exit 1

add_ld_preload()
{
  export LD_PRELOAD="${1}${LD_PRELOAD:+:}${LD_PRELOAD}"
}

add_ld_library_path()
{
  export LD_LIBRARY_PATH="${1}${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH}"
}

set_ld_environment()
{
  lib="datefudge.so"
  libpath="@LIBDIR@"
  set --  "$libpath"@MULTIARCH_PATTERN@"/datefudge/$lib"
  if [ ! -e "$1" ]; then
    echo "Cannot find $lib in $libpath" >&2
    [ -z "$POSH_VERSION" ] || echo "You might have just encountered posh bug#636601, please try using another shell." >&2
    exit 1;
  fi
  for path in "$@"; do
    add_ld_library_path "${path%/*}"
  done
  add_ld_preload "$lib"
  [ -z "$ld_preload_arg" ] || add_ld_preload "$ld_preload_arg"
}

set_datefudge_vars()
{
  if [ "$static" = "1" ]; then
    export DATEFUDGE=$sec1
    export DATEFUDGE_DOSTATIC="1"
  else
    export DATEFUDGE=$sec2
    unset  DATEFUDGE_DOSTATIC
  fi
}


set_ld_environment
set_datefudge_vars

exec "$@"
