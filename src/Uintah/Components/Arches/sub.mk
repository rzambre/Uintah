#
# Makefile fragment for this subdirectory
# $Id$
#

include $(SRCTOP)/scripts/smallso_prologue.mk

SRCDIR   := Uintah/Components/Arches

SRCS     += $(SRCDIR)/Arches.cc $(SRCDIR)/BoundaryCondition.cc \
	$(SRCDIR)/NonlinearSolver.cc $(SRCDIR)/PhysicalConstants.cc \
	$(SRCDIR)/PicardNonlinearSolver.cc \
	$(SRCDIR)/Properties.cc $(SRCDIR)/SmagorinskyModel.cc \
	$(SRCDIR)/TurbulenceModel.cc $(SRCDIR)/Discretization.cc \
	$(SRCDIR)/LinearSolver.cc \
	$(SRCDIR)/PressureSolver.cc $(SRCDIR)/MomentumSolver.cc \
	$(SRCDIR)/ScalarSolver.cc $(SRCDIR)/RBGSSolver.cc \
	$(SRCDIR)/Source.cc $(SRCDIR)/CellInformation.cc \
	$(SRCDIR)/ArchesLabel.cc $(SRCDIR)/ArchesVariables.cc


SUBDIRS := $(SRCDIR)/fortran

include $(SRCTOP)/scripts/recurse.mk

PSELIBS := Uintah/Parallel Uintah/Interface Uintah/Grid Uintah/Exceptions \
	   SCICore/Exceptions
LIBS := $(XML_LIBRARY) -lftn -lm
#LIBS += -lblas

include $(SRCTOP)/scripts/smallso_epilogue.mk

#
# $Log$
# Revision 1.19  2000/07/28 02:31:00  rawat
# moved all the labels in ArchesLabel. fixed some bugs and added matrix_dw to store matrix
# coeffecients
#
# Revision 1.17  2000/07/13 04:51:33  bbanerje
# Added pressureBC (bcp) .. now called bcpress.F (bcp.F removed)
#
# Revision 1.16  2000/07/02 05:47:31  bbanerje
# Uncommented all PerPatch and CellInformation stuff.
# Updated array sizes in inlbcs.F
#
# Revision 1.15  2000/06/14 20:40:50  rawat
# modified boundarycondition for physical boundaries and
# added CellInformation class
#
# Revision 1.14  2000/06/13 20:47:31  bbanerje
# Correct version of sub.mk (sub.mk removed from .cvsignore)
#
# Revision 1.13  2000/06/12 21:30:01  bbanerje
# Added first Fortran routines, added Stencil Matrix where needed,
# removed unnecessary CCVariables (e.g., sources etc.)
#
# Revision 1.12  2000/06/04 22:40:16  bbanerje
# Added Cocoon stuff, changed task, require, compute, get, put arguments
# to reflect new declarations. Changed sub.mk to include all the new files.
#
# Revision 1.11  2000/05/30 19:35:26  dav
# added SCICore/Exceptions to PSELIBS
#
# Revision 1.10  2000/05/18 16:14:39  sparker
# Commented out fortran compilation until we get the autoconf
#  part done
#
# Revision 1.9  2000/05/17 21:33:04  bbanerje
# Updated sub.mk to build library at this level with fortran routines.
#
# Revision 1.8  2000/04/13 20:05:52  sparker
# Compile more of arches
# Made SimulationController work somewhat
#
# Revision 1.7  2000/04/13 06:50:52  sparker
# More implementation to get this to work
#
# Revision 1.6  2000/04/12 22:58:30  sparker
# Resolved conflicts
# Making it compile
#
# Revision 1.5  2000/03/22 23:42:52  sparker
# Do not compile it all quite yet
#
# Revision 1.4  2000/03/22 23:41:20  sparker
# Working towards getting arches to compile/run
#
# Revision 1.3  2000/03/21 18:52:50  sparker
# Fixed compilation errors in Arches.cc
# use new problem spec class
#
# Revision 1.2  2000/03/20 19:38:20  sparker
# Added VPATH support
#
# Revision 1.1  2000/03/17 09:29:27  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
