# Makefile fragment for this subdirectory

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR   := Packages/Uintah/Core/Exceptions

SRCS     += \
	$(SRCDIR)/InvalidCompressionMode.cc \
	$(SRCDIR)/InvalidGrid.cc            \
	$(SRCDIR)/InvalidValue.cc           \
	$(SRCDIR)/ParameterNotFound.cc      \
	$(SRCDIR)/PetscError.cc             \
	$(SRCDIR)/ProblemSetupException.cc  \
	$(SRCDIR)/TypeMismatchException.cc  \
	$(SRCDIR)/VariableNotFoundInGrid.cc \
	$(SRCDIR)/OutFluxVolume.cc

PSELIBS := \
	Core/Exceptions \
	Dataflow/XMLUtil

LIBS := $(XML_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk

