# DWA build notes
These notes are relevant only to internal DWA builds.

This is the Arras 4 version of the 'arras_dataio' package. (It needs to
be defined as a separate package to avoid conflicts when built against the
current moonray folio, which uses 'arras_dataio' to implement the Arras 3
computations)

Data I/O (send, receive) related APIs between components also client.
Also includes merge computation related main logic.

Build
=====
## Rez-1 build with default ICC compiler:

    rez1
    setenv REZ_PACKAGES_PATH /usr/pic1/work/rez:$REZ_PACKAGES_PATH
    rez-env bart-6 -c "bart_setup -p ."
    rez-env bart-6 -c "bart_rez_build -d /usr/pic1/work/rez -- @install --compiler=icc150_64 --no-cache --type=opt-debug"
    

## Rez-1 build with GCC compiler:

    rez-env bart-6 -c "bart_rez_build  -d /usr/pic1/work/rez -- @install --compiler=gcc48_64 --no-cache --skip-install-clean --type=opt-debug"

## Rez-1 build with a pseudo-compiler: 

    rez-env bart-6 -c "bart_rez_build -d /usr/pic1/work/rez -- @install --pseudo-compiler=<pseudo-name>  --skip-install-clean --no-cache --type=opt-debug"
    
Example:

    rez-env bart-6 -c "bart_rez_build -d /usr/pic1/work/rez -- @install --pseudo-compiler=iccMaya2016_64  --skip-install-clean --no-cache --type=opt-debug"
    
## Rez-1 Doxygen build:

    rez-env bart-6 -c "bart_rez_build -d /usr/pic1/work/rez -- @install_doc --skip-install-clean --no-cache"
    

