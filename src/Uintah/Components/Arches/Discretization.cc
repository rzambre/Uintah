//----- Discretization.cc ----------------------------------------------

/* REFERENCED */
static char *id="@(#) $Id$";

#include <Uintah/Components/Arches/Arches.h>
#include <Uintah/Components/Arches/ArchesFort.h>
#include <Uintah/Components/Arches/Discretization.h>
#include <Uintah/Components/Arches/StencilMatrix.h>
#include <Uintah/Components/Arches/CellInformation.h>
#include <Uintah/Grid/Stencil.h>
#include <SCICore/Util/NotFinished.h>
#include <Uintah/Grid/Level.h>
#include <Uintah/Interface/Scheduler.h>
#include <Uintah/Interface/DataWarehouse.h>
#include <Uintah/Grid/Task.h>
#include <Uintah/Grid/SFCXVariable.h>
#include <Uintah/Grid/SFCYVariable.h>
#include <Uintah/Grid/SFCZVariable.h>
#include <Uintah/Grid/CCVariable.h>
#include <Uintah/Grid/PerPatch.h>
#include <Uintah/Grid/SoleVariable.h>
#include <SCICore/Geometry/Vector.h>
#include <Uintah/Exceptions/InvalidValue.h>
#include <Uintah/Grid/Array3.h>
#include <iostream>
using namespace std;

using namespace Uintah::ArchesSpace;
using SCICore::Geometry::Vector;

//****************************************************************************
// Default constructor for Discretization
//****************************************************************************
Discretization::Discretization()
{
}

//****************************************************************************
// Destructor
//****************************************************************************
Discretization::~Discretization()
{
}

//****************************************************************************
// Velocity stencil weights
//****************************************************************************
void 
Discretization::calculateVelocityCoeff(const ProcessorGroup* pc,
				       const Patch* patch,
				       DataWarehouseP&,
				       DataWarehouseP&,
				       double delta_t,
				       int index,
				       int eqnType, CellInformation* cellinfo,
				       ArchesVariables* coeff_vars)
{
  // Get the domain size 
  IntVector domLoU = coeff_vars->uVelocity.getFortLowIndex();
  IntVector domHiU = coeff_vars->uVelocity.getFortHighIndex();
  IntVector domLoV = coeff_vars->vVelocity.getFortLowIndex();
  IntVector domHiV = coeff_vars->vVelocity.getFortHighIndex();
  IntVector domLoW = coeff_vars->wVelocity.getFortLowIndex();
  IntVector domHiW = coeff_vars->wVelocity.getFortHighIndex();
  IntVector domLo = coeff_vars->density.getFortLowIndex();
  IntVector domHi = coeff_vars->density.getFortHighIndex();
  //IntVector idxLo = patch->getCellFORTLowIndex();
  //IntVector idxHi = patch->getCellFORTHighIndex();

  if (index == Arches::XDIR) {

    // Get the patch indices
    IntVector idxLoU = patch->getSFCXFORTLowIndex();
    IntVector idxHiU = patch->getSFCXFORTHighIndex();

    // Calculate the coeffs
    FORT_UVELCOEF(domLoU.get_pointer(), domHiU.get_pointer(),
		  idxLoU.get_pointer(), idxHiU.get_pointer(),
		  coeff_vars->uVelocity.getPointer(),
		  coeff_vars->uVelocityConvectCoeff[Arches::AE].getPointer(), 
		  coeff_vars->uVelocityConvectCoeff[Arches::AW].getPointer(), 
		  coeff_vars->uVelocityConvectCoeff[Arches::AN].getPointer(), 
		  coeff_vars->uVelocityConvectCoeff[Arches::AS].getPointer(), 
		  coeff_vars->uVelocityConvectCoeff[Arches::AT].getPointer(), 
		  coeff_vars->uVelocityConvectCoeff[Arches::AB].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AP].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AE].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AW].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AN].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AS].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AT].getPointer(), 
		  coeff_vars->uVelocityCoeff[Arches::AB].getPointer(), 
		  coeff_vars->variableCalledDU.getPointer(),
		  domLoV.get_pointer(), domHiV.get_pointer(),
		  coeff_vars->vVelocity.getPointer(),
		  domLoW.get_pointer(), domHiW.get_pointer(),
		  coeff_vars->wVelocity.getPointer(),
		  domLo.get_pointer(), domHi.get_pointer(),
		  coeff_vars->density.getPointer(),
		  coeff_vars->viscosity.getPointer(),
		  &delta_t,
		  cellinfo->ceeu.get_objs(), cellinfo->cweu.get_objs(),
		  cellinfo->cwwu.get_objs(),
		  cellinfo->cnn.get_objs(), cellinfo->csn.get_objs(),
		  cellinfo->css.get_objs(),
		  cellinfo->ctt.get_objs(), cellinfo->cbt.get_objs(),
		  cellinfo->cbb.get_objs(),
		  cellinfo->sewu.get_objs(), cellinfo->sew.get_objs(),
		  cellinfo->sns.get_objs(),
		  cellinfo->stb.get_objs(),
		  cellinfo->dxepu.get_objs(), cellinfo->dxpwu.get_objs(),
		  cellinfo->dxpw.get_objs(),
		  cellinfo->dynp.get_objs(), cellinfo->dyps.get_objs(),
		  cellinfo->dztp.get_objs(), cellinfo->dzpb.get_objs(),
		  cellinfo->fac1u.get_objs(), cellinfo->fac2u.get_objs(),
		  cellinfo->fac3u.get_objs(), cellinfo->fac4u.get_objs(),
		  cellinfo->iesdu.get_objs(), cellinfo->iwsdu.get_objs(), 
		  cellinfo->enfac.get_objs(), cellinfo->sfac.get_objs(),
		  cellinfo->tfac.get_objs(), cellinfo->bfac.get_objs());
  } else if (index == Arches::YDIR) {

    // Get the patch indices
    IntVector idxLoV = patch->getSFCYFORTLowIndex();
    IntVector idxHiV = patch->getSFCYFORTHighIndex();

    // Calculate the coeffs
    FORT_VVELCOEF(domLoV.get_pointer(), domHiV.get_pointer(),
		  idxLoV.get_pointer(), idxHiV.get_pointer(),
		  coeff_vars->vVelocity.getPointer(),
		  coeff_vars->vVelocityConvectCoeff[Arches::AE].getPointer(), 
		  coeff_vars->vVelocityConvectCoeff[Arches::AW].getPointer(), 
		  coeff_vars->vVelocityConvectCoeff[Arches::AN].getPointer(), 
		  coeff_vars->vVelocityConvectCoeff[Arches::AS].getPointer(), 
		  coeff_vars->vVelocityConvectCoeff[Arches::AT].getPointer(), 
		  coeff_vars->vVelocityConvectCoeff[Arches::AB].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AP].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AE].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AW].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AN].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AS].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AT].getPointer(), 
		  coeff_vars->vVelocityCoeff[Arches::AB].getPointer(), 
		  coeff_vars->variableCalledDV.getPointer(),
		  domLoU.get_pointer(), domHiU.get_pointer(),
		  coeff_vars->uVelocity.getPointer(),
		  domLoW.get_pointer(), domHiW.get_pointer(),
		  coeff_vars->wVelocity.getPointer(),
		  domLo.get_pointer(), domHi.get_pointer(),
		  coeff_vars->density.getPointer(),
		  coeff_vars->viscosity.getPointer(),
		  &delta_t,
		  cellinfo->cee.get_objs(), cellinfo->cwe.get_objs(),
		  cellinfo->cww.get_objs(),
		  cellinfo->cnnv.get_objs(), cellinfo->csnv.get_objs(),
		  cellinfo->cssv.get_objs(),
		  cellinfo->ctt.get_objs(), cellinfo->cbt.get_objs(),
		  cellinfo->cbb.get_objs(),
		  cellinfo->sew.get_objs(), cellinfo->snsv.get_objs(),
		  cellinfo->sns.get_objs(),
		  cellinfo->stb.get_objs(),
		  cellinfo->dxep.get_objs(), cellinfo->dxpw.get_objs(),
		  cellinfo->dynpv.get_objs(), cellinfo->dypsv.get_objs(),
		  cellinfo->dyps.get_objs(),
		  cellinfo->dztp.get_objs(), cellinfo->dzpb.get_objs(),
		  cellinfo->fac1v.get_objs(), cellinfo->fac2v.get_objs(),
		  cellinfo->fac3v.get_objs(), cellinfo->fac4v.get_objs(),
		  cellinfo->jnsdv.get_objs(), cellinfo->jssdv.get_objs(), 
		  cellinfo->efac.get_objs(), cellinfo->wfac.get_objs(),
		  cellinfo->tfac.get_objs(), cellinfo->bfac.get_objs());
  } else if (index == Arches::ZDIR) {

    // Get the patch indices
    IntVector idxLoW = patch->getSFCZFORTLowIndex();
    IntVector idxHiW = patch->getSFCZFORTHighIndex();

    // Calculate the coeffs
    FORT_WVELCOEF(domLoW.get_pointer(), domHiW.get_pointer(),
		  idxLoW.get_pointer(), idxHiW.get_pointer(),
		  coeff_vars->wVelocity.getPointer(),
		  coeff_vars->wVelocityConvectCoeff[Arches::AE].getPointer(), 
		  coeff_vars->wVelocityConvectCoeff[Arches::AW].getPointer(), 
		  coeff_vars->wVelocityConvectCoeff[Arches::AN].getPointer(), 
		  coeff_vars->wVelocityConvectCoeff[Arches::AS].getPointer(), 
		  coeff_vars->wVelocityConvectCoeff[Arches::AT].getPointer(), 
		  coeff_vars->wVelocityConvectCoeff[Arches::AB].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AP].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AE].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AW].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AN].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AS].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AT].getPointer(), 
		  coeff_vars->wVelocityCoeff[Arches::AB].getPointer(), 
		  coeff_vars->variableCalledDW.getPointer(),
		  domLoU.get_pointer(), domHiU.get_pointer(),
		  coeff_vars->uVelocity.getPointer(),
		  domLoV.get_pointer(), domHiV.get_pointer(),
		  coeff_vars->vVelocity.getPointer(),
		  domLo.get_pointer(), domHi.get_pointer(),
		  coeff_vars->density.getPointer(),
		  coeff_vars->viscosity.getPointer(),
		  &delta_t,
		  cellinfo->cee.get_objs(), cellinfo->cwe.get_objs(),
		  cellinfo->cww.get_objs(),
		  cellinfo->cnn.get_objs(), cellinfo->csn.get_objs(),
		  cellinfo->css.get_objs(),
		  cellinfo->cttw.get_objs(), cellinfo->cbtw.get_objs(),
		  cellinfo->cbbw.get_objs(),
		  cellinfo->sew.get_objs(), cellinfo->sns.get_objs(),
		  cellinfo->stbw.get_objs(), cellinfo->stb.get_objs(),
		  cellinfo->dxep.get_objs(), cellinfo->dxpw.get_objs(),
		  cellinfo->dynp.get_objs(), cellinfo->dyps.get_objs(),
		  cellinfo->dztpw.get_objs(), cellinfo->dzpbw.get_objs(),
		  cellinfo->dzpb.get_objs(),
		  cellinfo->fac1w.get_objs(), cellinfo->fac2w.get_objs(),
		  cellinfo->fac3w.get_objs(), cellinfo->fac4w.get_objs(),
		  cellinfo->ktsdw.get_objs(), cellinfo->kbsdw.get_objs(), 
		  cellinfo->efac.get_objs(), cellinfo->wfac.get_objs(),
		  cellinfo->enfac.get_objs(), cellinfo->sfac.get_objs());
  }

#ifdef MAY_BE_USEFUL_LATER  
  // int ioff = 1;
  // int joff = 0;
  // int koff = 0;

  // 3-d array for volume - fortran uses it for temporary storage
  // Array3<double> volume(patch->getLowIndex(), patch->getHighIndex());
  // FORT_VELCOEF(domLoU.get_pointer(), domHiU.get_pointer(),
  //       idxLoU.get_pointer(), idxHiU.get_pointer(),
  //       uVelocity.getPointer(),
  //       domLoV.get_pointer(), domHiV.get_pointer(),
  //       idxLoV.get_pointer(), idxHiV.get_pointer(),
  //       vVelocity.getPointer(),
  //       domLoW.get_pointer(), domHiW.get_pointer(),
  //       idxLoW.get_pointer(), idxHiW.get_pointer(),
  //       wVelocity.getPointer(),
  //       domLo.get_pointer(), domHi.get_pointer(),
  //       idxLo.get_pointer(), idxHi.get_pointer(),
  //       density.getPointer(),
  //       viscosity.getPointer(),
  //       uVelocityConvectCoeff[Arches::AP].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AE].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AW].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AN].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AS].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AT].getPointer(), 
  //       uVelocityConvectCoeff[Arches::AB].getPointer(), 
  //       uVelocityCoeff[Arches::AP].getPointer(), 
  //       uVelocityCoeff[Arches::AE].getPointer(), 
  //       uVelocityCoeff[Arches::AW].getPointer(), 
  //       uVelocityCoeff[Arches::AN].getPointer(), 
  //       uVelocityCoeff[Arches::AS].getPointer(), 
  //       uVelocityCoeff[Arches::AT].getPointer(), 
  //       uVelocityCoeff[Arches::AB].getPointer(), 
  //       delta_t,
  //       ioff, joff, koff, 
  //       cellinfo->ceeu, cellinfo->cweu, cellinfo->cwwu,
  //       cellinfo->cnn, cellinfo->csn, cellinfo->css,
  //       cellinfo->ctt, cellinfo->cbt, cellinfo->cbb,
  //       cellinfo->sewu, cellinfo->sns, cellinfo->stb,
  //       cellinfo->dxepu, cellinfo->dynp, cellinfo->dztp,
  //       cellinfo->dxpw, cellinfo->fac1u, cellinfo->fac2u,
  //       cellinfo->fac3u, cellinfo->fac4u,cellinfo->iesdu,
  //       cellinfo->iwsdu, cellinfo->enfac, cellinfo->sfac,
  //       cellinfo->tfac, cellinfo->bfac, volume);
#endif

}


//****************************************************************************
// Pressure stencil weights
//****************************************************************************
void 
Discretization::calculatePressureCoeff(const ProcessorGroup*,
				       const Patch* patch,
				       DataWarehouseP& old_dw,
				       DataWarehouseP& new_dw,
				       double delta_t, 
				       CellInformation* cellinfo,
				       ArchesVariables* coeff_vars)
{
  // Get the domain size and the patch indices
  IntVector domLo = coeff_vars->density.getFortLowIndex();
  IntVector domHi = coeff_vars->density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = coeff_vars->uVelocityCoeff[Arches::AP].getFortLowIndex();
  IntVector domHiU = coeff_vars->uVelocityCoeff[Arches::AP].getFortHighIndex();
  IntVector domLoV = coeff_vars->vVelocityCoeff[Arches::AP].getFortLowIndex();
  IntVector domHiV = coeff_vars->vVelocityCoeff[Arches::AP].getFortHighIndex();
  IntVector domLoW = coeff_vars->wVelocityCoeff[Arches::AP].getFortLowIndex();
  IntVector domHiW = coeff_vars->wVelocityCoeff[Arches::AP].getFortHighIndex();

  FORT_PRESSCOEFF(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  coeff_vars->density.getPointer(),
		  coeff_vars->pressCoeff[Arches::AE].getPointer(), 
		  coeff_vars->pressCoeff[Arches::AW].getPointer(), 
		  coeff_vars->pressCoeff[Arches::AN].getPointer(), 
		  coeff_vars->pressCoeff[Arches::AS].getPointer(), 
		  coeff_vars->pressCoeff[Arches::AT].getPointer(), 
		  coeff_vars->pressCoeff[Arches::AB].getPointer(), 
		  domLoU.get_pointer(), domHiU.get_pointer(),
		  coeff_vars->uVelocityCoeff[Arches::AP].getPointer(),
		  domLoV.get_pointer(), domHiV.get_pointer(),
		  coeff_vars->vVelocityCoeff[Arches::AP].getPointer(),
		  domLoW.get_pointer(), domHiW.get_pointer(),
		  coeff_vars->wVelocityCoeff[Arches::AP].getPointer(),
		  cellinfo->sew.get_objs(), cellinfo->sns.get_objs(), 
		  cellinfo->stb.get_objs(),
		  cellinfo->sewu.get_objs(), cellinfo->dxep.get_objs(), 
		  cellinfo->dxpw.get_objs(), 
		  cellinfo->snsv.get_objs(), cellinfo->dynp.get_objs(), 
		  cellinfo->dyps.get_objs(), 
		  cellinfo->stbw.get_objs(), cellinfo->dztp.get_objs(), 
		  cellinfo->dzpb.get_objs());


}
  
//****************************************************************************
// Scalar stencil weights
//****************************************************************************
void 
Discretization::calculateScalarCoeff(const ProcessorGroup* pc,
				     const Patch* patch,
				     DataWarehouseP& old_dw,
				     DataWarehouseP& new_dw,
				     double delta_t,
				     int index, 
				     CellInformation* cellinfo,
				     ArchesVariables* coeff_vars)
{

  // Get the domain size and the patch indices
  IntVector domLo = coeff_vars->density.getFortLowIndex();
  IntVector domHi = coeff_vars->density.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  IntVector domLoU = coeff_vars->uVelocity.getFortLowIndex();
  IntVector domHiU = coeff_vars->uVelocity.getFortHighIndex();
  IntVector domLoV = coeff_vars->vVelocity.getFortLowIndex();
  IntVector domHiV = coeff_vars->vVelocity.getFortHighIndex();
  IntVector domLoW = coeff_vars->wVelocity.getFortLowIndex();
  IntVector domHiW = coeff_vars->wVelocity.getFortHighIndex();
  
  FORT_SCALARCOEFF(domLo.get_pointer(), domHi.get_pointer(),
		   idxLo.get_pointer(), idxHi.get_pointer(),
		   coeff_vars->density.getPointer(),
		   coeff_vars->viscosity.getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AE].getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AW].getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AN].getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AS].getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AT].getPointer(), 
		   coeff_vars->scalarCoeff[Arches::AB].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AE].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AW].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AN].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AS].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AT].getPointer(), 
		   coeff_vars->scalarConvectCoeff[Arches::AB].getPointer(), 
		   domLoU.get_pointer(), domHiU.get_pointer(),
		   coeff_vars->uVelocity.getPointer(),
		   domLoV.get_pointer(), domHiV.get_pointer(),
		   coeff_vars->vVelocity.getPointer(),
		   domLoW.get_pointer(), domHiW.get_pointer(),
		   coeff_vars->wVelocity.getPointer(),
		   cellinfo->sew.get_objs(), cellinfo->sns.get_objs(), 
		   cellinfo->stb.get_objs(),
		   cellinfo->cee.get_objs(), cellinfo->cwe.get_objs(), 
		   cellinfo->cww.get_objs(),
		   cellinfo->cnn.get_objs(), cellinfo->csn.get_objs(), 
		   cellinfo->css.get_objs(),
		   cellinfo->ctt.get_objs(), cellinfo->cbt.get_objs(), 
		   cellinfo->cbb.get_objs(),
		   cellinfo->efac.get_objs(), cellinfo->wfac.get_objs(),
		   cellinfo->enfac.get_objs(), cellinfo->sfac.get_objs(),
		   cellinfo->tfac.get_objs(), cellinfo->bfac.get_objs(),
		   cellinfo->dxpw.get_objs(), cellinfo->dyps.get_objs(), 
		   cellinfo->dzpb.get_objs());
}

//****************************************************************************
// Calculate the diagonal terms (velocity)
//****************************************************************************
void 
Discretization::calculateVelDiagonal(const ProcessorGroup*,
				     const Patch* patch,
				     DataWarehouseP& old_dw,
				     DataWarehouseP& new_dw,
				     int index,
				     int eqnType,
				     ArchesVariables* coeff_vars)
{
  
  // Get the patch and variable indices
  IntVector domLo;
  IntVector domHi;
  IntVector idxLo;
  IntVector idxHi;
  switch(index) {
  case Arches::XDIR:
    domLo = coeff_vars->uVelLinearSrc.getFortLowIndex();
    domHi = coeff_vars->uVelLinearSrc.getFortHighIndex();
    idxLo = patch->getSFCXFORTLowIndex();
    idxHi = patch->getSFCXFORTHighIndex();
    FORT_APCAL(domLo.get_pointer(), domHi.get_pointer(),
	       idxLo.get_pointer(), idxHi.get_pointer(),
	       coeff_vars->uVelocityCoeff[Arches::AP].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AE].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AW].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AN].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AS].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AT].getPointer(), 
	       coeff_vars->uVelocityCoeff[Arches::AB].getPointer(),
	       coeff_vars->uVelLinearSrc.getPointer());
    break;
  case Arches::YDIR:
    domLo = coeff_vars->vVelLinearSrc.getFortLowIndex();
    domHi = coeff_vars->vVelLinearSrc.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();
    FORT_APCAL(domLo.get_pointer(), domHi.get_pointer(),
	       idxLo.get_pointer(), idxHi.get_pointer(),
	       coeff_vars->vVelocityCoeff[Arches::AP].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AE].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AW].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AN].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AS].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AT].getPointer(), 
	       coeff_vars->vVelocityCoeff[Arches::AB].getPointer(),
	       coeff_vars->vVelLinearSrc.getPointer());
    break;
  case Arches::ZDIR:
    domLo = coeff_vars->wVelLinearSrc.getFortLowIndex();
    domHi = coeff_vars->wVelLinearSrc.getFortHighIndex();
    idxLo = patch->getSFCZFORTLowIndex();
    idxHi = patch->getSFCZFORTHighIndex();
    FORT_APCAL(domLo.get_pointer(), domHi.get_pointer(),
	       idxLo.get_pointer(), idxHi.get_pointer(),
	       coeff_vars->wVelocityCoeff[Arches::AP].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AE].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AW].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AN].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AS].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AT].getPointer(), 
	       coeff_vars->wVelocityCoeff[Arches::AB].getPointer(),
	       coeff_vars->wVelLinearSrc.getPointer());
    break;
  default:
    throw InvalidValue("Invalid index in Discretization::calcVelDiagonal");
  }

}

//****************************************************************************
// Pressure diagonal
//****************************************************************************
void 
Discretization::calculatePressDiagonal(const ProcessorGroup*,
				       const Patch* patch,
				       DataWarehouseP& old_dw,
				       DataWarehouseP& new_dw,
				       ArchesVariables* coeff_vars) 
{
  
  // Get the domain size and the patch indices
  IntVector domLo = coeff_vars->pressLinearSrc.getFortLowIndex();
  IntVector domHi = coeff_vars->pressLinearSrc.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  // Calculate the diagonal terms (AP)
  FORT_APCAL(domLo.get_pointer(), domHi.get_pointer(),
	     idxLo.get_pointer(), idxHi.get_pointer(),
	     coeff_vars->pressCoeff[Arches::AP].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AE].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AW].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AN].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AS].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AT].getPointer(), 
	     coeff_vars->pressCoeff[Arches::AB].getPointer(),
	     coeff_vars->pressLinearSrc.getPointer());
}

//****************************************************************************
// Scalar diagonal
//****************************************************************************
void 
Discretization::calculateScalarDiagonal(const ProcessorGroup*,
					const Patch* patch,
					DataWarehouseP& old_dw,
					DataWarehouseP& new_dw,
					int index,
					ArchesVariables* coeff_vars)
{
  
  // Get the domain size and the patch indices
  IntVector domLo = coeff_vars->scalarLinearSrc.getFortLowIndex();
  IntVector domHi = coeff_vars->scalarLinearSrc.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  FORT_APCAL(domLo.get_pointer(), domHi.get_pointer(),
	     idxLo.get_pointer(), idxHi.get_pointer(),
	     coeff_vars->scalarCoeff[Arches::AP].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AE].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AW].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AN].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AS].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AT].getPointer(), 
	     coeff_vars->scalarCoeff[Arches::AB].getPointer(),
	     coeff_vars->scalarLinearSrc.getPointer());
}

//
// $Log$
// Revision 1.35  2000/07/28 02:30:59  rawat
// moved all the labels in ArchesLabel. fixed some bugs and added matrix_dw to store matrix
// coeffecients
//
// Revision 1.31  2000/07/14 03:45:45  rawat
// completed velocity bc and fixed some bugs
//
// Revision 1.30  2000/07/13 04:58:45  bbanerje
// Updated pressureDiagonal calc.
//
// Revision 1.29  2000/07/12 22:15:02  bbanerje
// Added pressure Coef .. will do until Kumar's code is up and running
//
// Revision 1.28  2000/07/12 19:55:43  bbanerje
// Added apcal stuff in calcVelDiagonal
//
// Revision 1.27  2000/07/11 15:46:27  rawat
// added setInitialGuess in PicardNonlinearSolver and also added uVelSrc
//
// Revision 1.26  2000/07/09 00:23:58  bbanerje
// Made changes to calcVelocitySource .. still getting seg violation here.
//
// Revision 1.25  2000/07/08 23:42:54  bbanerje
// Moved all enums to Arches.h and made corresponding changes.
//
// Revision 1.24  2000/07/08 23:08:54  bbanerje
// Added vvelcoef and wvelcoef ..
// Rawat check the ** WARNING ** tags in these files for possible problems.
//
// Revision 1.23  2000/07/08 08:03:33  bbanerje
// Readjusted the labels upto uvelcoef, removed bugs in CellInformation,
// made needed changes to uvelcoef.  Changed from StencilMatrix::AE etc
// to Arches::AE .. doesn't like enums in templates apparently.
//
// Revision 1.22  2000/07/07 23:07:45  rawat
// added inlet bc's
//
// Revision 1.21  2000/07/03 05:30:14  bbanerje
// Minor changes for inlbcs dummy code to compile and work. densitySIVBC is no more.
//
// Revision 1.20  2000/07/02 05:47:30  bbanerje
// Uncommented all PerPatch and CellInformation stuff.
// Updated array sizes in inlbcs.F
//
// Revision 1.19  2000/06/29 21:48:58  bbanerje
// Changed FC Vars to SFCX,Y,ZVars and added correct getIndex() to get domainhi/lo
// and index hi/lo
//
// Revision 1.18  2000/06/22 23:06:33  bbanerje
// Changed velocity related variables to FCVariable type.
// ** NOTE ** We may need 3 types of FCVariables (one for each direction)
//
// Revision 1.17  2000/06/21 07:50:59  bbanerje
// Corrected new_dw, old_dw problems, commented out intermediate dw (for now)
// and made the stuff go through schedule_time_advance.
//
// Revision 1.16  2000/06/18 01:20:15  bbanerje
// Changed names of varlabels in source to reflect the sequence of tasks.
// Result : Seg Violation in addTask in MomentumSolver
//
// Revision 1.15  2000/06/17 07:06:23  sparker
// Changed ProcessorContext to ProcessorGroup
//
// Revision 1.14  2000/06/14 20:40:48  rawat
// modified boundarycondition for physical boundaries and
// added CellInformation class
//
// Revision 1.13  2000/06/13 06:02:31  bbanerje
// Added some more StencilMatrices and vector<CCVariable> types.
//
// Revision 1.12  2000/06/07 06:13:54  bbanerje
// Changed CCVariable<Vector> to CCVariable<double> for most cases.
// Some of these variables may not be 3D Vectors .. they may be Stencils
// or more than 3D arrays. Need help here.
//
// Revision 1.11  2000/06/04 22:40:13  bbanerje
// Added Cocoon stuff, changed task, require, compute, get, put arguments
// to reflect new declarations. Changed sub.mk to include all the new files.
//
//
