#Makefile fragment for the Packages/BioPSE/Dataflow directory

include $(SRCTOP_ABS)/scripts/largeso_prologue.mk

SRCDIR := Packages/BioPSE/Dataflow
SUBDIRS := \
	$(SRCDIR)/GUI \
	$(SRCDIR)/Modules \

include $(SRCTOP_ABS)/scripts/recurse.mk

PSELIBS := 
LIBS := $(TK_LIBRARY) $(GL_LIBS) -lm

include $(SRCTOP_ABS)/scripts/largeso_epilogue.mk
