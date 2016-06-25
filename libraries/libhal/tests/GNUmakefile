ifndef CRYPTECH_ROOT
  CRYPTECH_ROOT := $(abspath ../../../../..)
endif

TFMDIR := $(abspath ../../libtfm)

vpath %.c ${CRYPTECH_ROOT}/sw/libhal/tests
vpath %.h ${CRYPTECH_ROOT}/sw/libhal/tests:${LIBTFM_DIR}

include ${CRYPTECH_ROOT}/sw/libhal/tests/GNUmakefile
