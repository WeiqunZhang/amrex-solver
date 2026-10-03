# Pass AMREX_HOME on the command line (build.sh does) or set it here.
AMREX_HOME ?= ../../../amrex

DEBUG = FALSE

USE_MPI  = TRUE
USE_OMP  = FALSE

COMP = gnu

DIM = 3

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

include ./Make.package

Pdirs := Base Boundary AmrCore LinearSolvers/MLMG

Ppack += $(foreach dir, $(Pdirs), $(AMREX_HOME)/Src/$(dir)/Make.package)

include $(Ppack)

include $(AMREX_HOME)/Tools/GNUMake/Make.rules
