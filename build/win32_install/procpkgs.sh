#!/bin/sh
#
# Generates a .nsh file for the NSIS Windows installer with MD5 checksums
# and file sizes of the UQM packages.
#
# Run this from an MSYS2 bash shell.
#
# The packages must be in the current directory.
#
# The awk, md5sum, and wc utilities must be installed, but MSYS2
# should generally already have those in your path.

NSH_FILE="packages.nsh"
PKG_VERSION="0.8.0"

CONTENT_PKG="uqm-$PKG_VERSION-content.uqm"
MUSIC_PKG="uqm-$PKG_VERSION-3domusic.uqm"
VOICE_PKG="uqm-$PKG_VERSION-voice.uqm"
REMIX1_PKG="uqm-remix-disc1.uqm"
REMIX2_PKG="uqm-remix-disc2.uqm"
REMIX3_PKG="uqm-remix-disc3.uqm"
REMIX4_PKG="uqm-remix-disc4.uqm"

check_pkg() {
    if [ ! -f "$1" ]; then
        echo "$1 not found."
        exit 1
    fi
}

process_pkg() {
    echo "Processing $1..."
    SUM=$(md5sum "$1" | awk '{print $1}')
    SZ=$(wc -c "$1" | awk '{print $1}')
    SZ=$(( SZ / 1024 ))
    {
        echo "!define $2_FILE \"$1\""
        echo "!define $2_MD5SUM \"$SUM\""
        echo "!define $2_SIZE $SZ"
    } >> $NSH_FILE
}

check_pkg $CONTENT_PKG
check_pkg $MUSIC_PKG
check_pkg $VOICE_PKG
check_pkg $REMIX1_PKG
check_pkg $REMIX2_PKG
check_pkg $REMIX3_PKG
check_pkg $REMIX4_PKG
check_pkg $UQM_EXE

echo "# Autogenerated by procpkgs.sh" > $NSH_FILE
echo "#" >> $NSH_FILE
process_pkg $CONTENT_PKG PKG_CONTENT
process_pkg $MUSIC_PKG PKG_3DOMUSIC
process_pkg $VOICE_PKG PKG_VOICE
process_pkg $REMIX1_PKG PKG_REMIX1
process_pkg $REMIX2_PKG PKG_REMIX2
process_pkg $REMIX3_PKG PKG_REMIX3
process_pkg $REMIX4_PKG PKG_REMIX4

echo "All packages processed. ${NSH_FILE} generated."
