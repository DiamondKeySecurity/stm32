ifndef CRYPTECH_ROOT
  CRYPTECH_ROOT := $(abspath ../../../..)
endif

REPO := ${CRYPTECH_ROOT}/sw/libhal

TFMDIR := $(abspath ../libtfm)

CFLAGS += -I${REPO}

vpath %.c ${REPO}
vpath %.h ${REPO}:${LIBTFM_DIR}

include ${CRYPTECH_ROOT}/sw/libhal/GNUmakefile
