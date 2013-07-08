#!/bin/sh
# Run this to generate all the initial makefiles, etc.
#
# NOTE: compare_versions() is stolen from gnome-autogen.sh

REQUIRED_VALA_VERSION=0.13.4

# Usage:
#     compare_versions MIN_VERSION ACTUAL_VERSION
# returns true if ACTUAL_VERSION >= MIN_VERSION
compare_versions() {
    ch_min_version=$1
    ch_actual_version=$2
    ch_status=0
    IFS="${IFS=         }"; ch_save_IFS="$IFS"; IFS="."
    set $ch_actual_version
    for ch_min in $ch_min_version; do
        ch_cur=`echo $1 | sed 's/[^0-9].*$//'`; shift # remove letter suffixes
        if [ -z "$ch_min" ]; then break; fi
        if [ -z "$ch_cur" ]; then ch_status=1; break; fi
        if [ $ch_cur -gt $ch_min ]; then break; fi
        if [ $ch_cur -lt $ch_min ]; then ch_status=1; break; fi
    done
    IFS="$ch_save_IFS"
    return $ch_status
}

# Vala version check
test -z "$VALAC" && VALAC=valac
VALA_VERSION=`$VALAC --version | cut -d" " -f2`

if [ -z "$VALA_VERSION" ]; then
    echo "**Error**: valac not installed or broken. You must have valac >= $REQUIRED_VALA_VERSION"
    echo "installed to build."
    exit 1
fi

echo "Found Vala $VALA_VERSION"

if ! compare_versions $REQUIRED_VALA_VERSION $VALA_VERSION; then
    echo "**Error**: You must have valac >= $REQUIRED_VALA_VERSION installed to build, you have $VALA_VERSION"
    exit 1
fi

# Generate required files
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  touch ChangeLog && # Automake requires that ChangeLog exist
  gtkdocize &&
  autopoint --force &&
  AUTOPOINT='intltoolize --automake --copy' autoreconf --verbose --force --install
) || exit

cfg="--disable-unit-tests
     --disable-upower
     --disable-hal
     --disable-gnome-keyring
     --disable-libsecret
     --disable-network-manager
     --disable-libiptcdata
     --disable-miner-rss
     --disable-miner-evolution
     --disable-miner-thunderbird
     --disable-miner-firefox
     --disable-nautilus-extension
     --disable-taglib
     --disable-tracker-needle
     --disable-tracker-preferences
     --disable-libxml2
     --disable-poppler
     --disable-libgxps
     --disable-libgsf
     --disable-libosinfo
     --disable-libcue
     --disable-playlist
     --disable-exempi

     --enable-libexif
     --enable-gdkpixbuf
     --enable-generic-media-extractor=gstreamer
     --enable-libgif
     --enable-libjpeg
     --enable-libtiff
     --enable-libvorbis
     --enable-libflac
     --enable-png-faster

     --with-unicode-support=libicu"

enable_tracker_ivi=true
processed=""
for idx in "$@"; do
  if [ "$idx" = "--disable-tracker-ivi" ]; then
    enable_tracker_ivi=false
  else
    processed="$processed "$idx
  fi
done

if $enable_tracker_ivi; then
  echo "
===============================================================================
  Tracker-IVI configuration enabled, this means certain configuration
  options have been set for you. If you wish to select options manually, do not
  use the --enable-tracker-ivi option.
===============================================================================
"
  processed="$processed $cfg"
fi


test -n "$NOCONFIGURE" || "$srcdir/configure" $processed
