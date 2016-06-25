ifndef CRYPTECH_ROOT
  CRYPTECH_ROOT := $(abspath ../../../..)
endif

REPO := ${CRYPTECH_ROOT}/user/ft/libcli

vpath %.c ${REPO}
vpath %.h ${REPO}

include ${REPO}/Makefile

# Kludge alert:
#
# We really should teach Makefiles that need libcli.h to look in the
# right place, but we don't want to wire the current location of
# libcli into too many Makefiles, since we intend to move it soon.
#
# So, for the moment, we just copy libcli.h.  Clean this up after the
# the repository moves (and maybe do something about the general
# assumption that header files and libraries always live in the same
# directory, that's wrong in a VPATH-based world).

all: $(abspath .)/libcli.h

$(abspath .)/libcli.h: ${REPO}/libcli.h
	cp -p $< $@
