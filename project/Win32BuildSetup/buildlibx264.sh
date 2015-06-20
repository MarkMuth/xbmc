#!/bin/bash
# This script is supposed to be called by /xbmc/tools/buildsteps/win32/make-mingwlibs.sh
# It downloads, extracts, configures, compiles and installs libx264

MAKEFLAGS=""
BGPROCESSFILE="$2"

BASE_URL=http://download.videolan.org/pub/videolan/x264/snapshots
VERSION=snapshot-20141218-2245
DOWNLOAD_LIBNAME=x264
LIBNAME=libx264

CUR_DIR=`pwd`
DEPS_DIR=$CUR_DIR/../BuildDependencies
LIB_DIR=$CUR_DIR/../../lib
WGET=$DEPS_DIR/bin/wget
UNZIP=$CUR_DIR/tools/7z/7za

cd $LIB_DIR

if [ "$1" == "clean" ]
then
  echo removing $LIBNAME
  if [ -d $LIBNAME ]
  then
    pushd $LIBNAME
    make distclean
	popd
    rm -r $LIBNAME
  fi
  if [ -f /xbmc/system/players/dvdplayer/libx264-142.dll ]
  then
    rm  /xbmc/system/players/dvdplayer/libx264-142.dll
  fi
fi

if [ ! -d $LIBNAME ]; then
  echo "libx264 is not installed yet."
  if [ ! -f $LIBNAME-$VERSION.tar.bz2 ]; then
    echo "libx264 is being downloaded."
    $WGET --no-check-certificate $BASE_URL/$DOWNLOAD_LIBNAME-$VERSION.tar.bz2 -O $LIBNAME-$VERSION.tar.bz2
  fi
  echo "libx264 is being extracted."
  $UNZIP x -y $LIBNAME-$VERSION.tar.bz2
  $UNZIP x -y $LIBNAME-$VERSION.tar
  mv $DOWNLOAD_LIBNAME-$VERSION $LIBNAME
  cd $LIBNAME
else
  #remove the bgprocessfile for signaling the process end
  rm $BGPROCESSFILE
  echo "libx264 is already installed."
  exit 0
fi

if [ $NUMBER_OF_PROCESSORS > 1 ]; then
  MAKEFLAGS=-j$NUMBER_OF_PROCESSORS
fi

echo "libx264 is being configured."
./configure \
  --prefix=/mingw \
  --disable-cli \
  --enable-shared &&

echo "libx264 is being compiled."
make $MAKEFLAGS &&

echo "libx264 is being installed."
# Install is required for building FFmpeg with H.264 encoding capabilities
make install &&

#strip libx264-142.dll &&
# Copying the libx264.dll is required for running Kodi
cp libx264-142.dll /xbmc/system/players/dvdplayer/libx264-142.dll &&
# Copying the pthreadGC2.dll is required for running Kodi (TODO get rid of this dependency somehow)
cp /mingw/bin/pthreadGC2.dll /xbmc/system/players/dvdplayer/pthreadGC2.dll &&

echo "libx264 has been installed."
#remove the bgprocessfile for signaling the process end
rm $BGPROCESSFILE
