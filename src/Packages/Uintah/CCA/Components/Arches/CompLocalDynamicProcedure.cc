//----- CompLocalDynamicProcedure.cc --------------------------------------------------

#include <TauProfilerForSCIRun.h>
#include <Packages/Uintah/CCA/Components/Arches/debug.h>
#include <Packages/Uintah/CCA/Components/Arches/CompLocalDynamicProcedure.h>
#include <Packages/Uintah/CCA/Components/Arches/PhysicalConstants.h>
#include <Packages/Uintah/CCA/Components/Arches/BoundaryCondition.h>
#include <Packages/Uintah/CCA/Components/Arches/CellInformation.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesLabel.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesMaterial.h>
#include <Packages/Uintah/CCA/Components/Arches/StencilMatrix.h>
#include <Packages/Uintah/CCA/Components/Arches/TimeIntegratorLabel.h>
#include <Packages/Uintah/Core/Grid/Variables/Stencil.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCXVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCYVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/SFCZVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/PerPatch.h>
#include <Packages/Uintah/Core/Grid/Variables/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Core/Geometry/Vector.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <Packages/Uintah/Core/Grid/Variables/Array3.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>

#include <Core/Thread/Time.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/MiscMath.h>

using namespace std;

using namespace Uintah;
using namespace SCIRun;
//#define use_fortran
#ifdef use_fortran
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_1loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_2loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_3loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_4loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_5loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_6loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_7loop_fort.h>
#include <Packages/Uintah/CCA/Components/Arches/fortran/comp_dynamic_8loop_fort.h>
#endif

// flag to enable filter check
// need even grid size, unfiltered values are +-1; filtered value should be 0
// #define FILTER_CHECK
#ifdef FILTER_CHECK
#include <Core/Math/MiscMath.h>
#endif

//****************************************************************************
// Default constructor for CompLocalDynamicProcedure
//****************************************************************************
CompLocalDynamicProcedure::CompLocalDynamicProcedure(const ArchesLabel* label, 
				   const MPMArchesLabel* MAlb,
				   PhysicalConstants* phyConsts,
				   BoundaryCondition* bndry_cond):
                                    TurbulenceModel(label, MAlb),
				    d_physicalConsts(phyConsts),
				    d_boundaryCondition(bndry_cond)
{
}

//****************************************************************************
// Destructor
//****************************************************************************
CompLocalDynamicProcedure::~CompLocalDynamicProcedure()
{
}

//****************************************************************************
//  Get the molecular viscosity from the Physical Constants object 
//****************************************************************************
double 
CompLocalDynamicProcedure::getMolecularViscosity() const {
  return d_physicalConsts->getMolecularViscosity();
}

//****************************************************************************
// Problem Setup 
//****************************************************************************
void 
CompLocalDynamicProcedure::problemSetup(const ProblemSpecP& params)
{
  ProblemSpecP db = params->findBlock("Turbulence");
  db->require("cf", d_CF);
  db->require("fac_mesh", d_factorMesh);
  db->require("filterl", d_filterl);
  db->require("var_const",d_CFVar); // const reqd by variance eqn
  // actually, Shmidt number, not Prandtl number
  db->getWithDefault("turbulentPrandtlNumber",d_turbPrNo,0.4);
  d_lower_limit = d_turbPrNo;
  db->getWithDefault("dynamicScalarModel",d_dynScalarModel,false);
  if (d_dynScalarModel)
   d_turbPrNo = 1.0; 
  db->getWithDefault("filter_cs_squared",d_filter_cs_squared,false);
#ifndef PetscFilter
  cout << "Filtering without Petsc is not supported in variable" << endl;
  cout << "density dynamic Smagorinsky model" << endl;
  exit(1);
#endif

}
void
CompLocalDynamicProcedure::initializeSmagCoeff( const ProcessorGroup*,
                                                const PatchSubset* patches,
                                                const MaterialSubset* ,
                                                DataWarehouse*,
                                                DataWarehouse* new_dw,
                                                const TimeIntegratorLabel* ) {
  int archIndex = 0; // only one arches material
  int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    CCVariable<double> Cs; //smag coeff 
    new_dw->allocateAndPut(Cs, d_lab->d_CsLabel, matlIndex, patch);  
    Cs.initialize(0.0);
  }
}

//****************************************************************************
// Schedule initialization of the smag coeff sub model 
//****************************************************************************
void 
CompLocalDynamicProcedure::sched_initializeSmagCoeff( SchedulerP& sched, 
                                                      const PatchSet* patches,
                                                      const MaterialSet* matls,
                                                      const TimeIntegratorLabel* timelabels )
{
  string taskname =  "CompLocalDynamicProcedure::initializeSmagCoeff" + timelabels->integrator_step_name;
  Task* tsk = scinew Task(taskname, this,
                          &CompLocalDynamicProcedure::initializeSmagCoeff,
                          timelabels);

  tsk->computes(d_lab->d_CsLabel);
  sched->addTask(tsk, patches, matls);
}



//****************************************************************************
// Schedule recomputation of the turbulence sub model 
//****************************************************************************
void 
CompLocalDynamicProcedure::sched_reComputeTurbSubmodel(SchedulerP& sched, 
					      const PatchSet* patches,
					      const MaterialSet* matls,
				         const TimeIntegratorLabel* timelabels)
{
  {
    string taskname =  "CompLocalDynamicProcedure::reComputeTurbSubmodel" +
		       timelabels->integrator_step_name;
    Task* tsk = scinew Task(taskname, this,
			    &CompLocalDynamicProcedure::reComputeTurbSubmodel,
			    timelabels);


    // Requires
    tsk->requires(Task::NewDW, d_lab->d_uVelocitySPBCLabel,
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_vVelocitySPBCLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_wVelocitySPBCLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_densityCPLabel, 
		  Ghost::AroundCells, Arches::TWOGHOSTCELLS);
    tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel, 
		  Ghost::AroundCells, Arches::TWOGHOSTCELLS);

    if (d_dynScalarModel) {
      if (d_calcScalar)
        tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      if (d_calcEnthalpy)
        tsk->requires(Task::NewDW, d_lab->d_enthalpySPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      if (d_calcReactingScalar)
        tsk->requires(Task::NewDW, d_lab->d_reactscalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
    }

    int mmWallID = d_boundaryCondition->getMMWallId();
    if (mmWallID > 0)
      tsk->requires(Task::NewDW, timelabels->ref_density);

    // Computes
  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    tsk->computes(d_lab->d_filterRhoULabel);
    tsk->computes(d_lab->d_filterRhoVLabel);
    tsk->computes(d_lab->d_filterRhoWLabel);
    tsk->computes(d_lab->d_filterRhoLabel);
    if (d_dynScalarModel) {
      if (d_calcScalar)
        tsk->computes(d_lab->d_filterRhoFLabel);
      if (d_calcEnthalpy)
        tsk->computes(d_lab->d_filterRhoELabel);
      if (d_calcReactingScalar)
        tsk->computes(d_lab->d_filterRhoRFLabel);
    }
  }
  else {
    tsk->modifies(d_lab->d_filterRhoULabel);
    tsk->modifies(d_lab->d_filterRhoVLabel);
    tsk->modifies(d_lab->d_filterRhoWLabel);
    tsk->modifies(d_lab->d_filterRhoLabel);
    if (d_dynScalarModel) {
      if (d_calcScalar)
        tsk->modifies(d_lab->d_filterRhoFLabel);
      if (d_calcEnthalpy)
        tsk->modifies(d_lab->d_filterRhoELabel);
      if (d_calcReactingScalar)
        tsk->modifies(d_lab->d_filterRhoRFLabel);
    }
  }  

    sched->addTask(tsk, patches, matls);
  }

  {
    string taskname =  "CompLocalDynamicProcedure::reComputeStrainRateTensors" +
		       timelabels->integrator_step_name;
    Task* tsk = scinew Task(taskname, this,
			    &CompLocalDynamicProcedure::reComputeStrainRateTensors,
			    timelabels);
    // Requires
    // Assuming one layer of ghost cells
    // initialize with the value of zero at the physical bc's
    // construct a stress tensor and stored as a array with the following order
    // {t11, t12, t13, t21, t22, t23, t31, t23, t33}
    tsk->requires(Task::NewDW, d_lab->d_uVelocitySPBCLabel,
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_vVelocitySPBCLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_wVelocitySPBCLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoULabel,
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoVLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoWLabel, 
		  Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoFLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        tsk->requires(Task::NewDW, d_lab->d_enthalpySPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoELabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        tsk->requires(Task::NewDW, d_lab->d_reactscalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoRFLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }
        
    // Computes
  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    tsk->computes(d_lab->d_strainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    tsk->computes(d_lab->d_filterStrainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->computes(d_lab->d_scalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->computes(d_lab->d_filterScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
      if (d_calcEnthalpy) {
        tsk->computes(d_lab->d_enthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->computes(d_lab->d_filterEnthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
      if (d_calcReactingScalar) {
        tsk->computes(d_lab->d_reactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->computes(d_lab->d_filterReactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
    }  
  }
  else {
    tsk->modifies(d_lab->d_strainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    tsk->modifies(d_lab->d_filterStrainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->modifies(d_lab->d_scalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->modifies(d_lab->d_filterScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
      if (d_calcEnthalpy) {
        tsk->modifies(d_lab->d_enthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->modifies(d_lab->d_filterEnthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
      if (d_calcReactingScalar) {
        tsk->modifies(d_lab->d_reactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
        tsk->modifies(d_lab->d_filterReactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain);
      }
    }  
  }  

    sched->addTask(tsk, patches, matls);
  }
  {
    string taskname =  "CompLocalDynamicProcedure::reComputeFilterValues" +
		       timelabels->integrator_step_name;
    Task* tsk = scinew Task(taskname, this,
			    &CompLocalDynamicProcedure::reComputeFilterValues,
			    timelabels);

    // Requires
    // Assuming one layer of ghost cells
    // initialize with the value of zero at the physical bc's
    // construct a stress tensor and stored as a array with the following order
    // {t11, t12, t13, t21, t22, t23, t31, t23, t33}
    
    tsk->requires(Task::NewDW, d_lab->d_newCCUVelocityLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_newCCVVelocityLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_newCCWVelocityLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_densityCPLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
//    int nofTimeSteps=d_lab->d_sharedState->getCurrentTopLevelTimeStep();
//    if(nofTimeSteps>1) {
//    tsk->requires(Task::NewDW, d_lab->d_CsLabel, 
//		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
//    }
    tsk->requires(Task::NewDW, d_lab->d_strainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterStrainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);

    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoFLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_scalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        tsk->requires(Task::NewDW, d_lab->d_enthalpySPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoELabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_enthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterEnthalpyGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        tsk->requires(Task::NewDW, d_lab->d_reactscalarSPLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterRhoRFLabel, 
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_reactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_filterReactScalarGradientCompLabel,
		      d_lab->d_vectorMatl, Task::OutOfDomain,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }  
    
    // Computes
  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    tsk->computes(d_lab->d_strainMagnitudeLabel);
    tsk->computes(d_lab->d_strainMagnitudeMLLabel);
    tsk->computes(d_lab->d_strainMagnitudeMMLabel);
    tsk->computes(d_lab->d_betaIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    tsk->computes(d_lab->d_LIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->computes(d_lab->d_scalarNumeratorLabel);
        tsk->computes(d_lab->d_scalarDenominatorLabel);
      }
      if (d_calcEnthalpy) {
        tsk->computes(d_lab->d_enthalpyNumeratorLabel);
        tsk->computes(d_lab->d_enthalpyDenominatorLabel);
      }
      if (d_calcReactingScalar) {
        tsk->computes(d_lab->d_reactScalarNumeratorLabel);
        tsk->computes(d_lab->d_reactScalarDenominatorLabel);
      }
    }      
  }
  else {
    tsk->modifies(d_lab->d_strainMagnitudeLabel);
    tsk->modifies(d_lab->d_strainMagnitudeMLLabel);
    tsk->modifies(d_lab->d_strainMagnitudeMMLabel);
    tsk->modifies(d_lab->d_betaIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    tsk->modifies(d_lab->d_LIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->modifies(d_lab->d_scalarNumeratorLabel);
        tsk->modifies(d_lab->d_scalarDenominatorLabel);
      }
      if (d_calcEnthalpy) {
        tsk->modifies(d_lab->d_enthalpyNumeratorLabel);
        tsk->modifies(d_lab->d_enthalpyDenominatorLabel);
      }
      if (d_calcReactingScalar) {
        tsk->modifies(d_lab->d_reactScalarNumeratorLabel);
        tsk->modifies(d_lab->d_reactScalarDenominatorLabel);
      }
    }      
  }
    
    sched->addTask(tsk, patches, matls);
  }
  {
    string taskname =  "CompLocalDynamicProcedure::reComputeSmagCoeff" +
		       timelabels->integrator_step_name;
    Task* tsk = scinew Task(taskname, this,
			    &CompLocalDynamicProcedure::reComputeSmagCoeff,
			    timelabels);

    // Requires
    // Assuming one layer of ghost cells
    // initialize with the value of zero at the physical bc's
    // construct a stress tensor and stored as an array with the following order
    // {t11, t12, t13, t21, t22, t23, t31, t23, t33}
    tsk->requires(Task::NewDW, d_lab->d_densityCPLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterRhoLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);

    tsk->requires(Task::NewDW, d_lab->d_strainMagnitudeLabel,
		  Ghost::None, Arches::ZEROGHOSTCELLS);
    tsk->requires(Task::NewDW, d_lab->d_strainMagnitudeMLLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_strainMagnitudeMMLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_filterStrainTensorCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_betaIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_LIJCompLabel,
		  d_lab->d_symTensorMatl, Task::OutOfDomain,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    tsk->requires(Task::OldDW, d_lab->d_CsLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->requires(Task::NewDW, d_lab->d_scalarNumeratorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_scalarDenominatorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        tsk->requires(Task::NewDW, d_lab->d_enthalpyNumeratorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_enthalpyDenominatorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        tsk->requires(Task::NewDW, d_lab->d_reactScalarNumeratorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
        tsk->requires(Task::NewDW, d_lab->d_reactScalarDenominatorLabel,
		      Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }      

    // for multimaterial
    if (d_MAlab)
      tsk->requires(Task::NewDW, d_lab->d_mmgasVolFracLabel, 
		    Ghost::None, Arches::ZEROGHOSTCELLS);
    
    // Computes
    tsk->modifies(d_lab->d_viscosityCTSLabel);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->modifies(d_lab->d_scalarDiffusivityLabel);
      }
      if (d_calcEnthalpy) {
        tsk->modifies(d_lab->d_enthalpyDiffusivityLabel);
      }
      if (d_calcReactingScalar) {
        tsk->modifies(d_lab->d_reactScalarDiffusivityLabel);
      }
    }      

  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    tsk->computes(d_lab->d_CsLabel);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->computes(d_lab->d_ShFLabel);
      }
      if (d_calcEnthalpy) {
        tsk->computes(d_lab->d_ShELabel);
      }
      if (d_calcReactingScalar) {
        tsk->computes(d_lab->d_ShRFLabel);
      }
    }      
  }
  else {
    tsk->modifies(d_lab->d_CsLabel);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tsk->modifies(d_lab->d_ShFLabel);
      }
      if (d_calcEnthalpy) {
        tsk->modifies(d_lab->d_ShELabel);
      }
      if (d_calcReactingScalar) {
        tsk->modifies(d_lab->d_ShRFLabel);
      }
    }      
  }
    
    sched->addTask(tsk, patches, matls);
  }
}


//****************************************************************************
// Actual recompute 
//****************************************************************************
void 
CompLocalDynamicProcedure::reComputeTurbSubmodel(const ProcessorGroup* pc,
					const PatchSubset* patches,
					const MaterialSubset*,
					DataWarehouse*,
					DataWarehouse* new_dw,
				        const TimeIntegratorLabel* timelabels)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constSFCXVariable<double> uVel;
    constSFCYVariable<double> vVel;
    constSFCZVariable<double> wVel;
    constCCVariable<double> density;
    constCCVariable<double> scalar;
    constCCVariable<double> enthalpy;
    constCCVariable<double> reactScalar;
    constCCVariable<int> cellType;

    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, matlIndex, patch)) 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    else {
      cellInfoP.setData(scinew CellInformation(patch));
      new_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    

    // Get the velocity
    new_dw->get(uVel, d_lab->d_uVelocitySPBCLabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(vVel, d_lab->d_vVelocitySPBCLabel, matlIndex, patch,
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(wVel, d_lab->d_wVelocitySPBCLabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(density, d_lab->d_densityCPLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::TWOGHOSTCELLS);
    if (d_dynScalarModel) {
      if (d_calcScalar)
        new_dw->get(scalar, d_lab->d_scalarSPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
      if (d_calcEnthalpy)
        new_dw->get(enthalpy, d_lab->d_enthalpySPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
      if (d_calcReactingScalar)
        new_dw->get(reactScalar, d_lab->d_reactscalarSPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
    }

    new_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::TWOGHOSTCELLS);

    
    SFCXVariable<double> filterRhoU;
    SFCYVariable<double> filterRhoV;
    SFCZVariable<double> filterRhoW;
    CCVariable<double> filterRho;
    CCVariable<double> filterRhoF;
    CCVariable<double> filterRhoE;
    CCVariable<double> filterRhoRF;
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
      new_dw->allocateAndPut(filterRhoU, 
			     d_lab->d_filterRhoULabel, matlIndex, patch);
      new_dw->allocateAndPut(filterRhoV, 
			     d_lab->d_filterRhoVLabel, matlIndex, patch);
      new_dw->allocateAndPut(filterRhoW, 
			     d_lab->d_filterRhoWLabel, matlIndex, patch);
      new_dw->allocateAndPut(filterRho, 
			     d_lab->d_filterRhoLabel, matlIndex, patch);
      if (d_dynScalarModel) {
        if (d_calcScalar)
          new_dw->allocateAndPut(filterRhoF, 
			         d_lab->d_filterRhoFLabel, matlIndex, patch);
        if (d_calcEnthalpy)
          new_dw->allocateAndPut(filterRhoE, 
			         d_lab->d_filterRhoELabel, matlIndex, patch);
        if (d_calcReactingScalar)
          new_dw->allocateAndPut(filterRhoRF, 
			         d_lab->d_filterRhoRFLabel, matlIndex, patch);
      }
    }
    else {
      new_dw->getModifiable(filterRhoU, 
			    d_lab->d_filterRhoULabel, matlIndex, patch);
      new_dw->getModifiable(filterRhoV, 
			    d_lab->d_filterRhoVLabel, matlIndex, patch);
      new_dw->getModifiable(filterRhoW, 
			    d_lab->d_filterRhoWLabel, matlIndex, patch);
      new_dw->getModifiable(filterRho, 
			    d_lab->d_filterRhoLabel, matlIndex, patch);
      if (d_dynScalarModel) {
        if (d_calcScalar)
          new_dw->getModifiable(filterRhoF, 
			        d_lab->d_filterRhoFLabel, matlIndex, patch);
        if (d_calcEnthalpy)
          new_dw->getModifiable(filterRhoE, 
			        d_lab->d_filterRhoELabel, matlIndex, patch);
        if (d_calcReactingScalar)
          new_dw->getModifiable(filterRhoRF, 
			        d_lab->d_filterRhoRFLabel, matlIndex, patch);
      }
    }
    filterRhoU.initialize(0.0);
    filterRhoV.initialize(0.0);
    filterRhoW.initialize(0.0);
    filterRho.initialize(0.0);
    if (d_dynScalarModel) {
      if (d_calcScalar)
        filterRhoF.initialize(0.0);
      if (d_calcEnthalpy)
        filterRhoE.initialize(0.0);
      if (d_calcReactingScalar)
        filterRhoRF.initialize(0.0);
    }

    bool xminus = patch->getBCType(Patch::xminus) != Patch::Neighbor;
    bool xplus =  patch->getBCType(Patch::xplus) != Patch::Neighbor;
    bool yminus = patch->getBCType(Patch::yminus) != Patch::Neighbor;
    bool yplus =  patch->getBCType(Patch::yplus) != Patch::Neighbor;
    bool zminus = patch->getBCType(Patch::zminus) != Patch::Neighbor;
    bool zplus =  patch->getBCType(Patch::zplus) != Patch::Neighbor;
    IntVector indexLowU = patch->getSFCXFORTLowIndex();
    IntVector indexHighU = patch->getSFCXFORTHighIndex();
    IntVector indexLowV = patch->getSFCYFORTLowIndex();
    IntVector indexHighV = patch->getSFCYFORTHighIndex();
    IntVector indexLowW = patch->getSFCZFORTLowIndex();
    IntVector indexHighW = patch->getSFCZFORTHighIndex();
    if (xminus) indexLowU -= IntVector(1,0,0); 
    if (yminus) indexLowV -= IntVector(0,1,0); 
    if (zminus) indexLowW -= IntVector(0,0,1); 
    if (xplus) indexHighU += IntVector(1,0,0); 
    if (yplus) indexHighV += IntVector(0,1,0); 
    if (zplus) indexHighW += IntVector(0,0,1); 

    int flowID = d_boundaryCondition->flowCellType();
    int mmWallID = d_boundaryCondition->getMMWallId();
    for (int colZ = indexLowU.z(); colZ <= indexHighU.z(); colZ ++) {
      for (int colY = indexLowU.y(); colY <= indexHighU.y(); colY ++) {
        for (int colX = indexLowU.x(); colX <= indexHighU.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  IntVector shift(0,0,0);
	  if ((xplus)&&((colX == indexHighU.x())||(colX == indexHighU.x()-1)))
	    shift = IntVector(-1,0,0);
	  int bndry_count=0;
	  if  (!(cellType[currCell + shift - IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  bool corner = (bndry_count==3);
	  double totalVol = 0.0;
	  if ((cellType[currCell+shift] == flowID)&&
	      (cellType[currCell+shift-IntVector(1,0,0)] != mmWallID)) {
	  for (int kk = -1; kk <= 1; kk ++) {
	    for (int jj = -1; jj <= 1; jj ++) {
	      for (int ii = -1; ii <= 1; ii ++) {
		IntVector filterCell = IntVector(colX+ii,colY+jj,colZ+kk);
		double vol = cellinfo->sew[colX+shift.x()+ii]*
			     cellinfo->sns[colY+jj]*
		             cellinfo->stb[colZ+kk];
		if (!(corner)) vol *= (1.0-0.5*abs(ii))*
		                      (1.0-0.5*abs(jj))*(1.0-0.5*abs(kk));
		// on the boundary
		if (cellType[filterCell+shift] != flowID) {
	          // intrusion
		  if (filterCell+shift == currCell+shift) {
		  // do nothing here, assuming intrusion velocity is 0
		  }
		}
		// inside the domain
		else
		  if (cellType[filterCell+shift-IntVector(1,0,0)] != mmWallID) {
		    filterRhoU[currCell] += vol*uVel[filterCell]*
			     0.5*(density[filterCell]+
			          density[filterCell-IntVector(1,0,0)]);
		    totalVol += vol;
		  }
	      }
	    }
	  }
	  filterRhoU[currCell] /= totalVol;
	  }
        }
      }
    }
    // assuming SFCY still stored with z being outer index and x inner index
    for (int colZ = indexLowV.z(); colZ <= indexHighV.z(); colZ ++) {
      for (int colY = indexLowV.y(); colY <= indexHighV.y(); colY ++) {
        for (int colX = indexLowV.x(); colX <= indexHighV.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  IntVector shift(0,0,0);
	  if ((yplus)&&((colY == indexHighV.y())||(colY == indexHighV.y()-1)))
            shift = IntVector(0,-1,0);
	  int bndry_count=0;
	  if  (!(cellType[currCell + shift - IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  bool corner = (bndry_count==3);
	  double totalVol = 0.0;
	  if ((cellType[currCell+shift] == flowID)&&
	      (cellType[currCell+shift-IntVector(0,1,0)] != mmWallID)) {
	  for (int kk = -1; kk <= 1; kk ++) {
	    for (int jj = -1; jj <= 1; jj ++) {
	      for (int ii = -1; ii <= 1; ii ++) {
		IntVector filterCell = IntVector(colX+ii,colY+jj,colZ+kk);
		double vol = cellinfo->sew[colX+ii]*
			     cellinfo->sns[colY+shift.y()+jj]*
		             cellinfo->stb[colZ+kk];
		if (!(corner)) vol *= (1.0-0.5*abs(ii))*
		                      (1.0-0.5*abs(jj))*(1.0-0.5*abs(kk));
		// on the boundary
		if (cellType[filterCell+shift] != flowID) {
	          // intrusion
		  if (filterCell+shift == currCell+shift) {
		  // do nothing here, assuming intrusion velocity is 0
		  }
		}
		// inside the domain
		else
		  if (cellType[filterCell+shift-IntVector(0,1,0)] != mmWallID) {
		    filterRhoV[currCell] += vol*vVel[filterCell]*
			     0.5*(density[filterCell]+
			          density[filterCell-IntVector(0,1,0)]);
		    totalVol += vol;
		  }
	      }
	    }
	  }

	  filterRhoV[currCell] /= totalVol;
	  }
        }
      }
    }
    // assuming SFCZ still stored with z being outer index and x inner index
    for (int colZ = indexLowW.z(); colZ <= indexHighW.z(); colZ ++) {
      for (int colY = indexLowW.y(); colY <= indexHighW.y(); colY ++) {
        for (int colX = indexLowW.x(); colX <= indexHighW.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  IntVector shift(0,0,0);
	  if ((zplus)&&((colZ == indexHighW.z())||(colZ == indexHighW.z()-1))) 
	    shift = IntVector(0,0,-1);
	  int bndry_count=0;
	  if  (!(cellType[currCell + shift - IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(1,0,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,1,0)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift - IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  if  (!(cellType[currCell + shift + IntVector(0,0,1)] == flowID))
		   bndry_count++;
	  bool corner = (bndry_count==3);
	  double totalVol = 0.0;
	  if ((cellType[currCell+shift] == flowID)&&
	      (cellType[currCell+shift-IntVector(0,0,1)] != mmWallID)) {
	  for (int kk = -1; kk <= 1; kk ++) {
	    for (int jj = -1; jj <= 1; jj ++) {
	      for (int ii = -1; ii <= 1; ii ++) {
		IntVector filterCell = IntVector(colX+ii,colY+jj,colZ+kk);
		double vol = cellinfo->sew[colX+ii]*
			     cellinfo->sns[colY+jj]*
		             cellinfo->stb[colZ+shift.z()+kk];
		if (!(corner)) vol *= (1.0-0.5*abs(ii))*
		                      (1.0-0.5*abs(jj))*(1.0-0.5*abs(kk));
		// on the boundary
		if (cellType[filterCell+shift] != flowID) {
	          // intrusion
		  if (filterCell+shift == currCell+shift) {
		  // do nothing here, assuming intrusion velocity is 0
		  }
		}
		// inside the domain
		else
		  if (cellType[filterCell+shift-IntVector(0,0,1)] != mmWallID) {
		    filterRhoW[currCell] += vol*wVel[filterCell]*
			     0.5*(density[filterCell]+
			          density[filterCell-IntVector(0,0,1)]);
		    totalVol += vol;
		  }
	      }
	    }
	  }

	  filterRhoW[currCell] /= totalVol;
	  }
        }
      }
    }

    IntVector idxLo = patch->getGhostCellLowIndex(Arches::ONEGHOSTCELL);
    IntVector idxHi = patch->getGhostCellHighIndex(Arches::ONEGHOSTCELL);
    Array3<double> rhoF(idxLo, idxHi);
    Array3<double> rhoE(idxLo, idxHi);
    Array3<double> rhoRF(idxLo, idxHi);
    rhoF.initialize(0.0);
    rhoE.initialize(0.0);
    rhoRF.initialize(0.0);
    int startZ = idxLo.z();
    if (zminus) startZ++;
    int endZ = idxHi.z();
    if (zplus) endZ--;
    int startY = idxLo.y();
    if (yminus) startY++;
    int endY = idxHi.y();
    if (yplus) endY--;
    int startX = idxLo.x();
    if (xminus) startX++;
    int endX = idxHi.x();
    if (xplus) endX--;
    for (int colZ = startZ; colZ < endZ; colZ ++) {
      for (int colY = startY; colY < endY; colY ++) {
	for (int colX = startX; colX < endX; colX ++) {
	  IntVector currCell(colX, colY, colZ);

          if (d_dynScalarModel) {
            if (d_calcScalar)
              rhoF[currCell] = density[currCell]*scalar[currCell];
            if (d_calcEnthalpy)
              rhoE[currCell] = density[currCell]*enthalpy[currCell];
            if (d_calcReactingScalar)
              rhoRF[currCell] = density[currCell]*reactScalar[currCell];
          }
        }
      }
    }

    filterRho.copy(density, patch->getCellLowIndex(),
		      patch->getCellHighIndex());
#ifdef PetscFilter
    d_filter->applyFilter(pc, patch, density, filterRho);
    // making filterRho nonzero 
    sum_vartype den_ref_var;
    if (mmWallID > 0) {
      new_dw->get(den_ref_var, timelabels->ref_density);

      idxLo = patch->getCellLowIndex();
      idxHi = patch->getCellHighIndex();

      for (int colZ = idxLo.z(); colZ < idxHi.z(); colZ ++) {
        for (int colY = idxLo.y(); colY < idxHi.y(); colY ++) {
	  for (int colX = idxLo.x(); colX < idxHi.x(); colX ++) {

	    IntVector currCell(colX, colY, colZ);

	    if (filterRho[currCell] < 1.0e-15) 
	      filterRho[currCell]=den_ref_var;

          }
        }
      }
    }
    if (d_dynScalarModel) {
      if (d_calcScalar)
        d_filter->applyFilter(pc, patch, rhoF, filterRhoF);
      if (d_calcEnthalpy)
        d_filter->applyFilter(pc, patch, rhoE, filterRhoE);
      if (d_calcReactingScalar)
        d_filter->applyFilter(pc, patch, rhoRF, filterRhoRF);
    }
#endif
  }
}
//****************************************************************************
// Actual recompute 
//****************************************************************************
void 
CompLocalDynamicProcedure::reComputeStrainRateTensors(const ProcessorGroup*,
					const PatchSubset* patches,
					const MaterialSubset*,
					DataWarehouse*,
					DataWarehouse* new_dw,
				        const TimeIntegratorLabel* timelabels)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constSFCXVariable<double> uVel;
    constSFCYVariable<double> vVel;
    constSFCZVariable<double> wVel;
    constSFCXVariable<double> filterRhoU;
    constSFCYVariable<double> filterRhoV;
    constSFCZVariable<double> filterRhoW;
    constCCVariable<double> scalar;
    constCCVariable<double> enthalpy;
    constCCVariable<double> reactScalar;
    constCCVariable<double> filterRho;
    constCCVariable<double> filterRhoF;
    constCCVariable<double> filterRhoE;
    constCCVariable<double> filterRhoRF;

    // Get the velocity
    new_dw->get(uVel, d_lab->d_uVelocitySPBCLabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(vVel, d_lab->d_vVelocitySPBCLabel, matlIndex, patch,
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(wVel, d_lab->d_wVelocitySPBCLabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(filterRhoU, d_lab->d_filterRhoULabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(filterRhoV, d_lab->d_filterRhoVLabel, matlIndex, patch,
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(filterRhoW, d_lab->d_filterRhoWLabel, matlIndex, patch, 
		Ghost::AroundFaces, Arches::ONEGHOSTCELL);
    new_dw->get(filterRho, d_lab->d_filterRhoLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->get(scalar, d_lab->d_scalarSPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterRhoF, d_lab->d_filterRhoFLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        new_dw->get(enthalpy, d_lab->d_enthalpySPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterRhoE, d_lab->d_filterRhoELabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        new_dw->get(reactScalar, d_lab->d_reactscalarSPLabel, matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterRhoRF, d_lab->d_filterRhoRFLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }

    // Get the PerPatch CellInformation data

    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, matlIndex, patch)) 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    else {
      cellInfoP.setData(scinew CellInformation(patch));
      new_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    
    
    // Get the patch and variable details
    // compatible with fortran index
    StencilMatrix<CCVariable<double> > SIJ;    //6 point tensor
    StencilMatrix<CCVariable<double> > filterSIJ;    //6 point tensor
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
      new_dw->allocateAndPut(SIJ[ii], 
			     d_lab->d_strainTensorCompLabel, ii, patch);
      new_dw->allocateAndPut(filterSIJ[ii], 
			     d_lab->d_filterStrainTensorCompLabel, ii, patch);
    }
    else {
      new_dw->getModifiable(SIJ[ii], 
			     d_lab->d_strainTensorCompLabel, ii, patch);
      new_dw->getModifiable(filterSIJ[ii], 
			     d_lab->d_filterStrainTensorCompLabel, ii, patch);
    }
      SIJ[ii].initialize(0.0);
      filterSIJ[ii].initialize(0.0);
    }
    StencilMatrix<CCVariable<double> > scalarGrad;    //vector
    StencilMatrix<CCVariable<double> > filterScalarGrad;    //vector
    StencilMatrix<CCVariable<double> > enthalpyGrad;    //vector
    StencilMatrix<CCVariable<double> > filterEnthalpyGrad;    //vector
    StencilMatrix<CCVariable<double> > reactScalarGrad;    //vector
    StencilMatrix<CCVariable<double> > filterReactScalarGrad;    //vector
    for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->allocateAndPut(scalarGrad[ii], 
			       d_lab->d_scalarGradientCompLabel, ii, patch);
        new_dw->allocateAndPut(filterScalarGrad[ii], 
			     d_lab->d_filterScalarGradientCompLabel, ii, patch);
      }
      if (d_calcEnthalpy) {
        new_dw->allocateAndPut(enthalpyGrad[ii], 
			       d_lab->d_enthalpyGradientCompLabel, ii, patch);
        new_dw->allocateAndPut(filterEnthalpyGrad[ii], 
			   d_lab->d_filterEnthalpyGradientCompLabel, ii, patch);
      }
      if (d_calcReactingScalar) {
        new_dw->allocateAndPut(reactScalarGrad[ii], 
			      d_lab->d_reactScalarGradientCompLabel, ii, patch);
        new_dw->allocateAndPut(filterReactScalarGrad[ii], 
			d_lab->d_filterReactScalarGradientCompLabel, ii, patch);
      }
    }
    }
    else {
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->getModifiable(scalarGrad[ii], 
			       d_lab->d_scalarGradientCompLabel, ii, patch);
        new_dw->getModifiable(filterScalarGrad[ii], 
			     d_lab->d_filterScalarGradientCompLabel, ii, patch);
      }
      if (d_calcEnthalpy) {
        new_dw->getModifiable(enthalpyGrad[ii], 
			       d_lab->d_enthalpyGradientCompLabel, ii, patch);
        new_dw->getModifiable(filterEnthalpyGrad[ii], 
			   d_lab->d_filterEnthalpyGradientCompLabel, ii, patch);
      }
      if (d_calcReactingScalar) {
        new_dw->getModifiable(reactScalarGrad[ii], 
			      d_lab->d_reactScalarGradientCompLabel, ii, patch);
        new_dw->getModifiable(filterReactScalarGrad[ii], 
			d_lab->d_filterReactScalarGradientCompLabel, ii, patch);
      }
    }
    }
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        scalarGrad[ii].initialize(0.0);
        filterScalarGrad[ii].initialize(0.0);
      }
      if (d_calcEnthalpy) {
        enthalpyGrad[ii].initialize(0.0);
        filterEnthalpyGrad[ii].initialize(0.0);
      }
      if (d_calcReactingScalar) {
        reactScalarGrad[ii].initialize(0.0);
        filterReactScalarGrad[ii].initialize(0.0);
      }
    }
    }

    IntVector indexLow = patch->getCellFORTLowIndex();
    IntVector indexHigh = patch->getCellFORTHighIndex();

#ifdef use_fortran
    fort_comp_dynamic_3loop(SIJ[0],SIJ[1],SIJ[2],SIJ[3],SIJ[4],SIJ[5],
        filterSIJ[0],filterSIJ[1],filterSIJ[2],
	filterSIJ[3],filterSIJ[4],filterSIJ[5],
	uVel,vVel,wVel, filterRhoU, filterRhoV, filterRhoW, filterRho,
	cellinfo->sew,cellinfo->sns,cellinfo->stb,
	indexLow,indexHigh);
    if (d_dynScalarModel)
    fort_comp_dynamic_6loop( scalarGrad[0], scalarGrad[1], scalarGrad[2],
	filterScalarGrad[0], filterScalarGrad[1], filterScalarGrad[2],
	enthalpyGrad[0], enthalpyGrad[1], enthalpyGrad[2],
	filterEnthalpyGrad[0], filterEnthalpyGrad[1], filterEnthalpyGrad[2],
	reactScalarGrad[0], reactScalarGrad[1], reactScalarGrad[2],
	filterReactScalarGrad[0], filterReactScalarGrad[1],
	filterReactScalarGrad[2],
	filterRho,
	scalar,filterRhoF, enthalpy, filterRhoE, reactScalar, filterRhoRF,
	cellinfo->sew,cellinfo->sns,cellinfo->stb,
	indexLow,indexHigh, d_calcScalar,
        d_calcEnthalpy, d_calcReactingScalar);
#else
    for (int colZ =indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double sewcur = cellinfo->sew[colX];
	  double snscur = cellinfo->sns[colY];
	  double stbcur = cellinfo->stb[colZ];

	  double uep, uwp, unp, usp, utp, ubp;
	  double vnp, vsp, vep, vwp, vtp, vbp;
	  double wtp, wbp, wep, wwp, wnp, wsp;

	  double uvelcur = uVel[currCell];
	  double vvelcur = vVel[currCell];
	  double wvelcur = wVel[currCell];
	  double uvelxp1 = uVel[IntVector(colX+1,colY,colZ)];
	  double vvelyp1 = vVel[IntVector(colX,colY+1,colZ)];
	  double wvelzp1 = wVel[IntVector(colX,colY,colZ+1)];

	  uep = uvelxp1;
	  uwp = uvelcur;
	  unp = 0.25*(uvelxp1 + uvelcur +
		      uVel[IntVector(colX+1,colY+1,colZ)] +
		      uVel[IntVector(colX,colY+1,colZ)]);
	  usp = 0.25*(uvelxp1 + uvelcur +
		      uVel[IntVector(colX+1,colY-1,colZ)] +
		      uVel[IntVector(colX,colY-1,colZ)]);
	  utp = 0.25*(uvelxp1 + uvelcur +
		      uVel[IntVector(colX+1,colY,colZ+1)] + 
		      uVel[IntVector(colX,colY,colZ+1)]);
	  ubp = 0.25*(uvelxp1 + uvelcur + 
		      uVel[IntVector(colX+1,colY,colZ-1)] + 
		      uVel[IntVector(colX,colY,colZ-1)]);

	  vnp = vvelyp1;
	  vsp = vvelcur;
	  vep = 0.25*(vvelyp1 + vvelcur +
		      vVel[IntVector(colX+1,colY+1,colZ)] + 
		      vVel[IntVector(colX+1,colY,colZ)]);
	  vwp = 0.25*(vvelyp1 + vvelcur +
		      vVel[IntVector(colX-1,colY+1,colZ)] + 
		      vVel[IntVector(colX-1,colY,colZ)]);
	  vtp = 0.25*(vvelyp1 + vvelcur + 
		      vVel[IntVector(colX,colY+1,colZ+1)] + 
		      vVel[IntVector(colX,colY,colZ+1)]);
	  vbp = 0.25*(vvelyp1 + vvelcur +
		      vVel[IntVector(colX,colY+1,colZ-1)] + 
		      vVel[IntVector(colX,colY,colZ-1)]);

	  wtp = wvelzp1;
	  wbp = wvelcur;
	  wep = 0.25*(wvelzp1 + wvelcur + 
		      wVel[IntVector(colX+1,colY,colZ+1)] + 
		      wVel[IntVector(colX+1,colY,colZ)]);
	  wwp = 0.25*(wvelzp1 + wvelcur +
		      wVel[IntVector(colX-1,colY,colZ+1)] + 
		      wVel[IntVector(colX-1,colY,colZ)]);
	  wnp = 0.25*(wvelzp1 + wvelcur + 
		      wVel[IntVector(colX,colY+1,colZ+1)] + 
		      wVel[IntVector(colX,colY+1,colZ)]);
	  wsp = 0.25*(wvelzp1 + wvelcur +
		      wVel[IntVector(colX,colY-1,colZ+1)] + 
		      wVel[IntVector(colX,colY-1,colZ)]);

	  //     calculate the grid strain rate tensor
	  (SIJ[0])[currCell] = (uep-uwp)/sewcur;
	  (SIJ[1])[currCell] = (vnp-vsp)/snscur;
	  (SIJ[2])[currCell] = (wtp-wbp)/stbcur;
	  (SIJ[3])[currCell] = 0.5*((unp-usp)/snscur + 
			       (vep-vwp)/sewcur);
	  (SIJ[4])[currCell] = 0.5*((utp-ubp)/stbcur + 
			       (wep-wwp)/sewcur);
	  (SIJ[5])[currCell] = 0.5*((vtp-vbp)/stbcur + 
			       (wnp-wsp)/snscur);

	  double fuep, fuwp, funp, fusp, futp, fubp;
	  double fvnp, fvsp, fvep, fvwp, fvtp, fvbp;
	  double fwtp, fwbp, fwep, fwwp, fwnp, fwsp;

	  double fuvelcur = filterRhoU[currCell]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX-1,colY,colZ)]));
	  double fvvelcur = filterRhoV[currCell]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX,colY-1,colZ)]));
	  double fwvelcur = filterRhoW[currCell]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX,colY,colZ-1)]));
	  double fuvelxp1 = filterRhoU[IntVector(colX+1,colY,colZ)]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX+1,colY,colZ)]));
	  double fvvelyp1 = filterRhoV[IntVector(colX,colY+1,colZ)]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX,colY+1,colZ)]));
	  double fwvelzp1 = filterRhoW[IntVector(colX,colY,colZ+1)]/
		            (0.5*(filterRho[currCell] +
			     filterRho[IntVector(colX,colY,colZ+1)]));

	  fuep = fuvelxp1;
	  fuwp = fuvelcur;
	  funp = 0.25*(fuvelxp1 + fuvelcur +
		      filterRhoU[IntVector(colX+1,colY+1,colZ)]/ 
		      (0.5*(filterRho[IntVector(colX,colY+1,colZ)] +
		       filterRho[IntVector(colX+1,colY+1,colZ)])) +
		      filterRhoU[IntVector(colX,colY+1,colZ)]/
		      (0.5*(filterRho[IntVector(colX,colY+1,colZ)] +
		       filterRho[IntVector(colX-1,colY+1,colZ)])));
	  fusp = 0.25*(fuvelxp1 + fuvelcur +
		      filterRhoU[IntVector(colX+1,colY-1,colZ)]/
		      (0.5*(filterRho[IntVector(colX,colY-1,colZ)] +
		       filterRho[IntVector(colX+1,colY-1,colZ)])) +
		      filterRhoU[IntVector(colX,colY-1,colZ)]/
		      (0.5*(filterRho[IntVector(colX,colY-1,colZ)] +
		       filterRho[IntVector(colX-1,colY-1,colZ)])));
	  futp = 0.25*(fuvelxp1 + fuvelcur +
		      filterRhoU[IntVector(colX+1,colY,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ+1)] +
		       filterRho[IntVector(colX+1,colY,colZ+1)])) +
		      filterRhoU[IntVector(colX,colY,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ+1)] +
		       filterRho[IntVector(colX-1,colY,colZ+1)])));
	  fubp = 0.25*(fuvelxp1 + fuvelcur + 
		      filterRhoU[IntVector(colX+1,colY,colZ-1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ-1)] +
		       filterRho[IntVector(colX+1,colY,colZ-1)])) +
		      filterRhoU[IntVector(colX,colY,colZ-1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ-1)] +
		       filterRho[IntVector(colX-1,colY,colZ-1)])));

	  fvnp = fvvelyp1;
	  fvsp = fvvelcur;
	  fvep = 0.25*(fvvelyp1 + fvvelcur +
		      filterRhoV[IntVector(colX+1,colY+1,colZ)]/
		      (0.5*(filterRho[IntVector(colX+1,colY,colZ)] +
		       filterRho[IntVector(colX+1,colY+1,colZ)])) +
		      filterRhoV[IntVector(colX+1,colY,colZ)]/
		      (0.5*(filterRho[IntVector(colX+1,colY,colZ)] +
		       filterRho[IntVector(colX+1,colY-1,colZ)])));
	  fvwp = 0.25*(fvvelyp1 + fvvelcur +
		      filterRhoV[IntVector(colX-1,colY+1,colZ)]/
		      (0.5*(filterRho[IntVector(colX-1,colY,colZ)] +
		       filterRho[IntVector(colX-1,colY+1,colZ)])) +
		      filterRhoV[IntVector(colX-1,colY,colZ)]/
		      (0.5*(filterRho[IntVector(colX-1,colY,colZ)] +
		       filterRho[IntVector(colX-1,colY-1,colZ)])));
	  fvtp = 0.25*(fvvelyp1 + fvvelcur + 
		      filterRhoV[IntVector(colX,colY+1,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ+1)] +
		       filterRho[IntVector(colX,colY+1,colZ+1)])) +
		      filterRhoV[IntVector(colX,colY,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ+1)] +
		       filterRho[IntVector(colX,colY-1,colZ+1)])));
	  fvbp = 0.25*(fvvelyp1 + fvvelcur +
		      filterRhoV[IntVector(colX,colY+1,colZ-1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ-1)] +
		       filterRho[IntVector(colX,colY+1,colZ-1)])) +
		      filterRhoV[IntVector(colX,colY,colZ-1)]/
		      (0.5*(filterRho[IntVector(colX,colY,colZ-1)] +
		       filterRho[IntVector(colX,colY-1,colZ-1)])));

	  fwtp = fwvelzp1;
	  fwbp = fwvelcur;
	  fwep = 0.25*(fwvelzp1 + fwvelcur + 
		      filterRhoW[IntVector(colX+1,colY,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX+1,colY,colZ)] +
		       filterRho[IntVector(colX+1,colY,colZ+1)])) +
		      filterRhoW[IntVector(colX+1,colY,colZ)]/
		      (0.5*(filterRho[IntVector(colX+1,colY,colZ)] +
		       filterRho[IntVector(colX+1,colY,colZ-1)])));
	  fwwp = 0.25*(fwvelzp1 + fwvelcur +
		      filterRhoW[IntVector(colX-1,colY,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX-1,colY,colZ)] +
		       filterRho[IntVector(colX-1,colY,colZ+1)])) +
		      filterRhoW[IntVector(colX-1,colY,colZ)]/
		      (0.5*(filterRho[IntVector(colX-1,colY,colZ)] +
		       filterRho[IntVector(colX-1,colY,colZ-1)])));
	  fwnp = 0.25*(fwvelzp1 + fwvelcur + 
		      filterRhoW[IntVector(colX,colY+1,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY+1,colZ)] +
		       filterRho[IntVector(colX,colY+1,colZ+1)])) +
		      filterRhoW[IntVector(colX,colY+1,colZ)]/
		      (0.5*(filterRho[IntVector(colX,colY+1,colZ)] +
		       filterRho[IntVector(colX,colY+1,colZ-1)])));
	  fwsp = 0.25*(fwvelzp1 + fwvelcur +
		      filterRhoW[IntVector(colX,colY-1,colZ+1)]/
		      (0.5*(filterRho[IntVector(colX,colY-1,colZ)] +
		       filterRho[IntVector(colX,colY-1,colZ+1)])) +
		      filterRhoW[IntVector(colX,colY-1,colZ)]/
		      (0.5*(filterRho[IntVector(colX,colY-1,colZ)] +
		       filterRho[IntVector(colX,colY-1,colZ-1)])));

	  //     calculate the filtered strain rate tensor
	  (filterSIJ[0])[currCell] = (fuep-fuwp)/sewcur;
	  (filterSIJ[1])[currCell] = (fvnp-fvsp)/snscur;
	  (filterSIJ[2])[currCell] = (fwtp-fwbp)/stbcur;
	  (filterSIJ[3])[currCell] = 0.5*((funp-fusp)/snscur + 
			             (fvep-fvwp)/sewcur);
	  (filterSIJ[4])[currCell] = 0.5*((futp-fubp)/stbcur + 
			             (fwep-fwwp)/sewcur);
	  (filterSIJ[5])[currCell] = 0.5*((fvtp-fvbp)/stbcur + 
			             (fwnp-fwsp)/snscur);

	  double scalarxp, scalarxm, scalaryp;
	  double scalarym, scalarzp, scalarzm;
	  double scalarcurr;
	  double fscalarxp, fscalarxm, fscalaryp;
	  double fscalarym, fscalarzp, fscalarzm;
	  double fscalarcurr;
	  double enthalpyxp, enthalpyxm, enthalpyyp;
	  double enthalpyym, enthalpyzp, enthalpyzm;
	  double enthalpycurr;
	  double fenthalpyxp, fenthalpyxm, fenthalpyyp;
	  double fenthalpyym, fenthalpyzp, fenthalpyzm;
	  double fenthalpycurr;
	  double reactScalarxp, reactScalarxm, reactScalaryp;
	  double reactScalarym, reactScalarzp, reactScalarzm;
	  double reactScalarcurr;
	  double freactScalarxp, freactScalarxm, freactScalaryp;
	  double freactScalarym, freactScalarzp, freactScalarzm;
	  double freactScalarcurr;

          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalarcurr = scalar[currCell];

	      scalarxm = 0.5*(scalarcurr+
			      scalar[IntVector(colX-1,colY,colZ)]);
	      scalarxp = 0.5*(scalarcurr+
			      scalar[IntVector(colX+1,colY,colZ)]);
	      scalarym = 0.5*(scalarcurr+
			      scalar[IntVector(colX,colY-1,colZ)]);
	      scalaryp = 0.5*(scalarcurr+
			      scalar[IntVector(colX,colY+1,colZ)]);
	      scalarzm = 0.5*(scalarcurr+
			      scalar[IntVector(colX,colY,colZ-1)]);
	      scalarzp = 0.5*(scalarcurr+
			      scalar[IntVector(colX,colY,colZ+1)]);

	      (scalarGrad[0])[currCell] = (scalarxp-scalarxm)/sewcur;
	      (scalarGrad[1])[currCell] = (scalaryp-scalarym)/snscur;
	      (scalarGrad[2])[currCell] = (scalarzp-scalarzm)/stbcur;

	      fscalarcurr = filterRhoF[currCell]/filterRho[currCell];

	      fscalarxm = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX-1,colY,colZ)]/
		              filterRho[IntVector(colX-1,colY,colZ)]);
	      fscalarxp = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX+1,colY,colZ)]/
			      filterRho[IntVector(colX+1,colY,colZ)]);
	      fscalarym = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX,colY-1,colZ)]/
			      filterRho[IntVector(colX,colY-1,colZ)]);
	      fscalaryp = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX,colY+1,colZ)]/
			      filterRho[IntVector(colX,colY+1,colZ)]);
	      fscalarzm = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX,colY,colZ-1)]/
			      filterRho[IntVector(colX,colY,colZ-1)]);
	      fscalarzp = 0.5*(fscalarcurr+
			      filterRhoF[IntVector(colX,colY,colZ+1)]/
			      filterRho[IntVector(colX,colY,colZ+1)]);

	      (filterScalarGrad[0])[currCell] = (fscalarxp-fscalarxm)/sewcur;
	      (filterScalarGrad[1])[currCell] = (fscalaryp-fscalarym)/snscur;
	      (filterScalarGrad[2])[currCell] = (fscalarzp-fscalarzm)/stbcur;
            }
            if (d_calcEnthalpy) {
	      enthalpycurr = enthalpy[currCell];

	      enthalpyxm = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX-1,colY,colZ)]);
	      enthalpyxp = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX+1,colY,colZ)]);
	      enthalpyym = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX,colY-1,colZ)]);
	      enthalpyyp = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX,colY+1,colZ)]);
	      enthalpyzm = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX,colY,colZ-1)]);
	      enthalpyzp = 0.5*(enthalpycurr+
			      enthalpy[IntVector(colX,colY,colZ+1)]);

	      (enthalpyGrad[0])[currCell] = (enthalpyxp-enthalpyxm)/sewcur;
	      (enthalpyGrad[1])[currCell] = (enthalpyyp-enthalpyym)/snscur;
	      (enthalpyGrad[2])[currCell] = (enthalpyzp-enthalpyzm)/stbcur;

	      fenthalpycurr = filterRhoE[currCell]/filterRho[currCell];

	      fenthalpyxm = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX-1,colY,colZ)]/
		              filterRho[IntVector(colX-1,colY,colZ)]);
	      fenthalpyxp = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX+1,colY,colZ)]/
			      filterRho[IntVector(colX+1,colY,colZ)]);
	      fenthalpyym = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX,colY-1,colZ)]/
			      filterRho[IntVector(colX,colY-1,colZ)]);
	      fenthalpyyp = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX,colY+1,colZ)]/
			      filterRho[IntVector(colX,colY+1,colZ)]);
	      fenthalpyzm = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX,colY,colZ-1)]/
			      filterRho[IntVector(colX,colY,colZ-1)]);
	      fenthalpyzp = 0.5*(fenthalpycurr+
			      filterRhoE[IntVector(colX,colY,colZ+1)]/
			      filterRho[IntVector(colX,colY,colZ+1)]);

	      (filterEnthalpyGrad[0])[currCell] = (fenthalpyxp-fenthalpyxm)/
		                                  sewcur;
	      (filterEnthalpyGrad[1])[currCell] = (fenthalpyyp-fenthalpyym)/
		                                  snscur;
	      (filterEnthalpyGrad[2])[currCell] = (fenthalpyzp-fenthalpyzm)/
		                                  stbcur;
            }
            if (d_calcReactingScalar) {
	      reactScalarcurr = reactScalar[currCell];

	      reactScalarxm = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX-1,colY,colZ)]);
	      reactScalarxp = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX+1,colY,colZ)]);
	      reactScalarym = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX,colY-1,colZ)]);
	      reactScalaryp = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX,colY+1,colZ)]);
	      reactScalarzm = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX,colY,colZ-1)]);
	      reactScalarzp = 0.5*(reactScalarcurr+
			      reactScalar[IntVector(colX,colY,colZ+1)]);

	      (reactScalarGrad[0])[currCell] = (reactScalarxp-reactScalarxm)/
		                               sewcur;
	      (reactScalarGrad[1])[currCell] = (reactScalaryp-reactScalarym)/
		                               snscur;
	      (reactScalarGrad[2])[currCell] = (reactScalarzp-reactScalarzm)/
		                               stbcur;

	      freactScalarcurr = filterRhoRF[currCell]/filterRho[currCell];

	      freactScalarxm = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX-1,colY,colZ)]/
		              filterRho[IntVector(colX-1,colY,colZ)]);
	      freactScalarxp = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX+1,colY,colZ)]/
			      filterRho[IntVector(colX+1,colY,colZ)]);
	      freactScalarym = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX,colY-1,colZ)]/
			      filterRho[IntVector(colX,colY-1,colZ)]);
	      freactScalaryp = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX,colY+1,colZ)]/
			      filterRho[IntVector(colX,colY+1,colZ)]);
	      freactScalarzm = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX,colY,colZ-1)]/
			      filterRho[IntVector(colX,colY,colZ-1)]);
	      freactScalarzp = 0.5*(freactScalarcurr+
			      filterRhoRF[IntVector(colX,colY,colZ+1)]/
			      filterRho[IntVector(colX,colY,colZ+1)]);

	      (filterReactScalarGrad[0])[currCell] = 
		      (freactScalarxp-freactScalarxm)/sewcur;
	      (filterReactScalarGrad[1])[currCell] = 
		      (freactScalaryp-freactScalarym)/snscur;
	      (filterReactScalarGrad[2])[currCell] = 
		      (freactScalarzp-freactScalarzm)/stbcur;
            }
          }
	}
      }
    }
#endif
  }
}



//****************************************************************************
// Actual recompute 
//****************************************************************************
void 
CompLocalDynamicProcedure::reComputeFilterValues(const ProcessorGroup* pc,
					const PatchSubset* patches,
					const MaterialSubset*,
					DataWarehouse*,
					DataWarehouse* new_dw,
				        const TimeIntegratorLabel* timelabels)
{
  int nofTimeSteps=d_lab->d_sharedState->getCurrentTopLevelTimeStep();
  for (int p = 0; p < patches->size(); p++) {
  TAU_PROFILE_TIMER(compute1, "Compute1", "[reComputeFilterValues::compute1]" , TAU_USER);
  TAU_PROFILE_TIMER(compute2, "Compute2", "[reComputeFilterValues::compute2]" , TAU_USER);
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constCCVariable<double> ccUVel;
    constCCVariable<double> ccVVel;
    constCCVariable<double> ccWVel;
    constCCVariable<double> den;
    constCCVariable<double> scalar;
    constCCVariable<double> enthalpy;
    constCCVariable<double> reactScalar;
    constCCVariable<double> filterRho;
    constCCVariable<double> filterRhoF;
    constCCVariable<double> filterRhoE;
    constCCVariable<double> filterRhoRF;
    constCCVariable<double> CsStar;

    // Get the velocity and density
    new_dw->get(ccUVel, d_lab->d_newCCUVelocityLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(ccVVel, d_lab->d_newCCVVelocityLabel, matlIndex, patch,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(ccWVel, d_lab->d_newCCWVelocityLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(den, d_lab->d_densityCPLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(filterRho, d_lab->d_filterRhoLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
      new_dw->get(scalar, d_lab->d_scalarSPLabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      new_dw->get(filterRhoF, d_lab->d_filterRhoFLabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
      new_dw->get(enthalpy, d_lab->d_enthalpySPLabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      new_dw->get(filterRhoE, d_lab->d_filterRhoELabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
      new_dw->get(reactScalar, d_lab->d_reactscalarSPLabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      new_dw->get(filterRhoRF, d_lab->d_filterRhoRFLabel, matlIndex, patch, 
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }  

    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, matlIndex, patch)) 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    else {
      cellInfoP.setData(scinew CellInformation(patch));
      new_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    
    
    IntVector idxLo = patch->getGhostCellLowIndex(Arches::ONEGHOSTCELL);
    IntVector idxHi = patch->getGhostCellHighIndex(Arches::ONEGHOSTCELL);

    StencilMatrix<constCCVariable<double> > SIJ; //6 point tensor
    StencilMatrix<constCCVariable<double> > SHATIJ; //6 point tensor
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
      new_dw->get(SIJ[ii], d_lab->d_strainTensorCompLabel, ii, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
      new_dw->get(SHATIJ[ii], d_lab->d_filterStrainTensorCompLabel,
		  ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
    }

    StencilMatrix<constCCVariable<double> > scalarGrad; //vector
    StencilMatrix<constCCVariable<double> > filterScalarGrad; //vector
    StencilMatrix<constCCVariable<double> > enthalpyGrad; //vector
    StencilMatrix<constCCVariable<double> > filterEnthalpyGrad; //vector
    StencilMatrix<constCCVariable<double> > reactScalarGrad; //vector
    StencilMatrix<constCCVariable<double> > filterReactScalarGrad; //vector
    for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->get(scalarGrad[ii], d_lab->d_scalarGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterScalarGrad[ii],
		    d_lab->d_filterScalarGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        new_dw->get(enthalpyGrad[ii], d_lab->d_enthalpyGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterEnthalpyGrad[ii],
		    d_lab->d_filterEnthalpyGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        new_dw->get(reactScalarGrad[ii], d_lab->d_reactScalarGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(filterReactScalarGrad[ii],
		    d_lab->d_filterReactScalarGradientCompLabel,
		    ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }  
    }

    StencilMatrix<Array3<double> > betaIJ;  //6 point tensor
    StencilMatrix<Array3<double> > betaHATIJ; //6 point tensor
    //  0-> 11, 1->22, 2->33, 3 ->12, 4->13, 5->23
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
      betaIJ[ii].resize(idxLo, idxHi);
      betaIJ[ii].initialize(0.0);
      betaHATIJ[ii].resize(idxLo, idxHi);
      betaHATIJ[ii].initialize(0.0);
    }  // allocate stress tensor coeffs
    StencilMatrix<Array3<double> > scalarBeta;  //vector
    StencilMatrix<Array3<double> > scalarBetaHat; //vector
    StencilMatrix<Array3<double> > enthalpyBeta;  //vector
    StencilMatrix<Array3<double> > enthalpyBetaHat; //vector
    StencilMatrix<Array3<double> > reactScalarBeta;  //vector
    StencilMatrix<Array3<double> > reactScalarBetaHat; //vector
    for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        scalarBeta[ii].resize(idxLo, idxHi);
        scalarBeta[ii].initialize(0.0);
        scalarBetaHat[ii].resize(idxLo, idxHi);
        scalarBetaHat[ii].initialize(0.0);
      }
      if (d_calcEnthalpy) {
        enthalpyBeta[ii].resize(idxLo, idxHi);
        enthalpyBeta[ii].initialize(0.0);
        enthalpyBetaHat[ii].resize(idxLo, idxHi);
        enthalpyBetaHat[ii].initialize(0.0);
      }
      if (d_calcReactingScalar) {
        reactScalarBeta[ii].resize(idxLo, idxHi);
        reactScalarBeta[ii].initialize(0.0);
        reactScalarBetaHat[ii].resize(idxLo, idxHi);
        reactScalarBetaHat[ii].initialize(0.0);
      }
    }  
    }

    CCVariable<double> IsImag;
    CCVariable<double> MLI;
    CCVariable<double> MMI;
    CCVariable<double> scalarNum;
    CCVariable<double> scalarDenom;
    CCVariable<double> enthalpyNum;
    CCVariable<double> enthalpyDenom;
    CCVariable<double> reactScalarNum;
    CCVariable<double> reactScalarDenom;
    StencilMatrix<CCVariable<double> > betaij;    //6 point tensor
    StencilMatrix<CCVariable<double> > lij;    //6 point tensor
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
      new_dw->allocateAndPut(betaij[ii], 
			     d_lab->d_betaIJCompLabel, ii, patch);
      new_dw->allocateAndPut(lij[ii], 
			     d_lab->d_LIJCompLabel, ii, patch);
    }
    else {
      new_dw->getModifiable(betaij[ii], 
			     d_lab->d_betaIJCompLabel, ii, patch);
      new_dw->getModifiable(lij[ii], 
			     d_lab->d_LIJCompLabel, ii, patch);
    }
      betaij[ii].initialize(0.0);
      lij[ii].initialize(0.0);
    }
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
    new_dw->allocateAndPut(IsImag, 
			   d_lab->d_strainMagnitudeLabel, matlIndex, patch);
    new_dw->allocateAndPut(MLI, 
			   d_lab->d_strainMagnitudeMLLabel, matlIndex, patch);
    new_dw->allocateAndPut(MMI, 
			   d_lab->d_strainMagnitudeMMLabel, matlIndex, patch);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->allocateAndPut(scalarNum, 
			  d_lab->d_scalarNumeratorLabel, matlIndex, patch);
        new_dw->allocateAndPut(scalarDenom, 
			  d_lab->d_scalarDenominatorLabel, matlIndex, patch);
      }
      if (d_calcEnthalpy) {
        new_dw->allocateAndPut(enthalpyNum, 
			  d_lab->d_enthalpyNumeratorLabel, matlIndex, patch);
        new_dw->allocateAndPut(enthalpyDenom, 
			  d_lab->d_enthalpyDenominatorLabel, matlIndex, patch);
      }
      if (d_calcReactingScalar) {
        new_dw->allocateAndPut(reactScalarNum, 
			d_lab->d_reactScalarNumeratorLabel, matlIndex, patch);
        new_dw->allocateAndPut(reactScalarDenom, 
			d_lab->d_reactScalarDenominatorLabel, matlIndex, patch);
      }
    }  
    }
    else {
    new_dw->getModifiable(IsImag, 
			   d_lab->d_strainMagnitudeLabel, matlIndex, patch);
    new_dw->getModifiable(MLI, 
			   d_lab->d_strainMagnitudeMLLabel, matlIndex, patch);
    new_dw->getModifiable(MMI, 
			   d_lab->d_strainMagnitudeMMLabel, matlIndex, patch);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->getModifiable(scalarNum, 
			  d_lab->d_scalarNumeratorLabel, matlIndex, patch);
        new_dw->getModifiable(scalarDenom, 
			  d_lab->d_scalarDenominatorLabel, matlIndex, patch);
      }
      if (d_calcEnthalpy) {
        new_dw->getModifiable(enthalpyNum, 
			  d_lab->d_enthalpyNumeratorLabel, matlIndex, patch);
        new_dw->getModifiable(enthalpyDenom, 
			  d_lab->d_enthalpyDenominatorLabel, matlIndex, patch);
      }
      if (d_calcReactingScalar) {
        new_dw->getModifiable(reactScalarNum, 
			d_lab->d_reactScalarNumeratorLabel, matlIndex, patch);
        new_dw->getModifiable(reactScalarDenom, 
			d_lab->d_reactScalarDenominatorLabel, matlIndex, patch);
      }
    }  
    }
    IsImag.initialize(0.0);
    MLI.initialize(0.0);
    MMI.initialize(0.0);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        scalarNum.initialize(0.0);
        scalarDenom.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        enthalpyNum.initialize(0.0);
        enthalpyDenom.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        reactScalarNum.initialize(0.0);
        reactScalarDenom.initialize(0.0);
      }
    }  


    // compute test filtered velocities, density and product 
    // (den*u*u, den*u*v, den*u*w, den*v*v,
    // den*v*w, den*w*w)
    // using a box filter, generalize it to use other filters such as Gaussian


    Array3<double> IsI(idxLo, idxHi); // magnitude of strain rate
    Array3<double> rhoU(idxLo, idxHi);
    Array3<double> rhoV(idxLo, idxHi);
    Array3<double> rhoW(idxLo, idxHi);
    Array3<double> rhoUU(idxLo, idxHi);
    Array3<double> rhoUV(idxLo, idxHi);
    Array3<double> rhoUW(idxLo, idxHi);
    Array3<double> rhoVV(idxLo, idxHi);
    Array3<double> rhoVW(idxLo, idxHi);
    Array3<double> rhoWW(idxLo, idxHi);
    IsI.initialize(0.0);
    rhoU.initialize(0.0);
    rhoV.initialize(0.0);
    rhoW.initialize(0.0);
    rhoUU.initialize(0.0);
    rhoUV.initialize(0.0);
    rhoUW.initialize(0.0);
    rhoVV.initialize(0.0);
    rhoVW.initialize(0.0);
    rhoWW.initialize(0.0);
    Array3<double> rhoFU;
    Array3<double> rhoFV;
    Array3<double> rhoFW;
    Array3<double> rhoEU;
    Array3<double> rhoEV;
    Array3<double> rhoEW;
    Array3<double> rhoRFU;
    Array3<double> rhoRFV;
    Array3<double> rhoRFW;
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        rhoFU.resize(idxLo, idxHi);
        rhoFU.initialize(0.0);
        rhoFV.resize(idxLo, idxHi);
        rhoFV.initialize(0.0);
        rhoFW.resize(idxLo, idxHi);
        rhoFW.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        rhoEU.resize(idxLo, idxHi);
        rhoEU.initialize(0.0);
        rhoEV.resize(idxLo, idxHi);
        rhoEV.initialize(0.0);
        rhoEW.resize(idxLo, idxHi);
        rhoEW.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        rhoRFU.resize(idxLo, idxHi);
        rhoRFU.initialize(0.0);
        rhoRFV.resize(idxLo, idxHi);
        rhoRFV.initialize(0.0);
        rhoRFW.resize(idxLo, idxHi);
        rhoRFW.initialize(0.0);
      }
    }  
    bool xminus = patch->getBCType(Patch::xminus) != Patch::Neighbor;
    bool xplus =  patch->getBCType(Patch::xplus) != Patch::Neighbor;
    bool yminus = patch->getBCType(Patch::yminus) != Patch::Neighbor;
    bool yplus =  patch->getBCType(Patch::yplus) != Patch::Neighbor;
    bool zminus = patch->getBCType(Patch::zminus) != Patch::Neighbor;
    bool zplus =  patch->getBCType(Patch::zplus) != Patch::Neighbor;
    int startZ = idxLo.z();
    if (zminus) startZ++;
    int endZ = idxHi.z();
    if (zplus) endZ--;
    int startY = idxLo.y();
    if (yminus) startY++;
    int endY = idxHi.y();
    if (yplus) endY--;
    int startX = idxLo.x();
    if (xminus) startX++;
    int endX = idxHi.x();
    if (xplus) endX--;

  TAU_PROFILE_START(compute1);
#ifdef use_fortran
    IntVector start(startX, startY, startZ);
    IntVector end(endX - 1, endY - 1, endZ -1);
    fort_comp_dynamic_1loop(SIJ[0],SIJ[1],SIJ[2],SIJ[3],SIJ[4],SIJ[5],
	ccUVel,ccVVel,ccWVel,den,
	IsI,betaIJ[0],betaIJ[1],betaIJ[2],betaIJ[3],betaIJ[4],betaIJ[5],
	rhoU,rhoV,rhoW,rhoUU,rhoUV,rhoUW,rhoVV,rhoVW,rhoWW,
	start,end);
    if (d_dynScalarModel)
    fort_comp_dynamic_4loop(ccUVel,ccVVel,ccWVel,
        den,scalar,enthalpy,reactScalar,
	scalarGrad[0], scalarGrad[1],scalarGrad[2],
	enthalpyGrad[0], enthalpyGrad[1],enthalpyGrad[2],
	reactScalarGrad[0], reactScalarGrad[1],reactScalarGrad[2], IsI,
	scalarBeta[0],scalarBeta[1],scalarBeta[2],
	enthalpyBeta[0],enthalpyBeta[1],enthalpyBeta[2],
	reactScalarBeta[0],reactScalarBeta[1],reactScalarBeta[2],
	rhoFU,rhoFV,rhoFW,rhoEU,rhoEV,rhoEW,rhoRFU,rhoRFV,rhoRFW,
	start,end,d_calcScalar,
        d_calcEnthalpy,d_calcReactingScalar);
#else
    for (int colZ = startZ; colZ < endZ; colZ ++) {
      for (int colY = startY; colY < endY; colY ++) {
	for (int colX = startX; colX < endX; colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  // calculate absolute value of the grid strain rate
          // computes for the ghost cells too
	  double sij0 = (SIJ[0])[currCell];
	  double sij1 = (SIJ[1])[currCell];
	  double sij2 = (SIJ[2])[currCell];
	  double sij3 = (SIJ[3])[currCell];
	  double sij4 = (SIJ[4])[currCell];
	  double sij5 = (SIJ[5])[currCell];
	  double isi_cur = sqrt(2.0*(sij0*sij0 + sij1*sij1 + sij2*sij2 +
				     2.0*(sij3*sij3 + sij4*sij4 + sij5*sij5)));
// trace has been neglected
	  double trace = (sij0 + sij1 + sij2)/3.0;
//	  double trace = 0.0;
	  double uvel_cur = ccUVel[currCell];
	  double vvel_cur = ccVVel[currCell];
	  double wvel_cur = ccWVel[currCell];
	  double den_cur = den[currCell];

	  IsI[currCell] = isi_cur; 

	  //    calculate the grid filtered stress tensor, beta

	  (betaIJ[0])[currCell] = den_cur*isi_cur*(sij0-trace);
	  (betaIJ[1])[currCell] = den_cur*isi_cur*(sij1-trace);
	  (betaIJ[2])[currCell] = den_cur*isi_cur*(sij2-trace);
	  (betaIJ[3])[currCell] = den_cur*isi_cur*sij3;
	  (betaIJ[4])[currCell] = den_cur*isi_cur*sij4;
	  (betaIJ[5])[currCell] = den_cur*isi_cur*sij5;
	  (betaij[0])[currCell] = (betaIJ[0])[currCell];
	  (betaij[1])[currCell] = (betaIJ[1])[currCell];
	  (betaij[2])[currCell] = (betaIJ[2])[currCell];
	  (betaij[3])[currCell] = (betaIJ[3])[currCell];
	  (betaij[4])[currCell] = (betaIJ[4])[currCell];
	  (betaij[5])[currCell] = (betaIJ[5])[currCell];
	  // required to compute Leonard term
	  rhoUU[currCell] = den_cur*uvel_cur*uvel_cur;
	  rhoUV[currCell] = den_cur*uvel_cur*vvel_cur;
	  rhoUW[currCell] = den_cur*uvel_cur*wvel_cur;
	  rhoVV[currCell] = den_cur*vvel_cur*vvel_cur;
	  rhoVW[currCell] = den_cur*vvel_cur*wvel_cur;
	  rhoWW[currCell] = den_cur*wvel_cur*wvel_cur;
	  rhoU[currCell] = den_cur*uvel_cur;
	  rhoV[currCell] = den_cur*vvel_cur;
	  rhoW[currCell] = den_cur*wvel_cur;

	  double scalar_cur,enthalpy_cur,reactScalar_cur;
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalar_cur = scalar[currCell];
	      (scalarBeta[0])[currCell] = den_cur*isi_cur*(scalarGrad[0])[currCell];
	      (scalarBeta[1])[currCell] = den_cur*isi_cur*(scalarGrad[1])[currCell];
	      (scalarBeta[2])[currCell] = den_cur*isi_cur*(scalarGrad[2])[currCell];
	      rhoFU[currCell] = den_cur*scalar_cur*uvel_cur;
	      rhoFV[currCell] = den_cur*scalar_cur*vvel_cur;
	      rhoFW[currCell] = den_cur*scalar_cur*wvel_cur;
            }
            if (d_calcEnthalpy) {
	      enthalpy_cur = enthalpy[currCell];
	      (enthalpyBeta[0])[currCell] = den_cur*isi_cur*(enthalpyGrad[0])[currCell];
	      (enthalpyBeta[1])[currCell] = den_cur*isi_cur*(enthalpyGrad[1])[currCell];
	      (enthalpyBeta[2])[currCell] = den_cur*isi_cur*(enthalpyGrad[2])[currCell];
	      rhoEU[currCell] = den_cur*enthalpy_cur*uvel_cur;
	      rhoEV[currCell] = den_cur*enthalpy_cur*vvel_cur;
	      rhoEW[currCell] = den_cur*enthalpy_cur*wvel_cur;
            }
            if (d_calcReactingScalar) {
	      reactScalar_cur = reactScalar[currCell];
	      (reactScalarBeta[0])[currCell] = den_cur*isi_cur*(reactScalarGrad[0])[currCell];
	      (reactScalarBeta[1])[currCell] = den_cur*isi_cur*(reactScalarGrad[1])[currCell];
	      (reactScalarBeta[2])[currCell] = den_cur*isi_cur*(reactScalarGrad[2])[currCell];
	      rhoRFU[currCell] = den_cur*reactScalar_cur*uvel_cur;
	      rhoRFV[currCell] = den_cur*reactScalar_cur*vvel_cur;
	      rhoRFW[currCell] = den_cur*reactScalar_cur*wvel_cur;
            }
          }  
	}
      }
    }
#endif
  TAU_PROFILE_STOP(compute1);
    Array3<double> filterRhoUU(patch->getLowIndex(), patch->getHighIndex());
    filterRhoUU.initialize(0.0);
    Array3<double> filterRhoUV(patch->getLowIndex(), patch->getHighIndex());
    filterRhoUV.initialize(0.0);
    Array3<double> filterRhoUW(patch->getLowIndex(), patch->getHighIndex());
    filterRhoUW.initialize(0.0);
    Array3<double> filterRhoVV(patch->getLowIndex(), patch->getHighIndex());
    filterRhoVV.initialize(0.0);
    Array3<double> filterRhoVW(patch->getLowIndex(), patch->getHighIndex());
    filterRhoVW.initialize(0.0);
    Array3<double> filterRhoWW(patch->getLowIndex(), patch->getHighIndex());
    filterRhoWW.initialize(0.0);
    Array3<double> filterRhoU(patch->getLowIndex(), patch->getHighIndex());
    filterRhoU.initialize(0.0);
    Array3<double> filterRhoV(patch->getLowIndex(), patch->getHighIndex());
    filterRhoV.initialize(0.0);
    Array3<double> filterRhoW(patch->getLowIndex(), patch->getHighIndex());
    filterRhoW.initialize(0.0);

    Array3<double> filterRhoFU;
    Array3<double> filterRhoFV;
    Array3<double> filterRhoFW;
    Array3<double> filterRhoEU;
    Array3<double> filterRhoEV;
    Array3<double> filterRhoEW;
    Array3<double> filterRhoRFU;
    Array3<double> filterRhoRFV;
    Array3<double> filterRhoRFW;
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        filterRhoFU.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoFU.initialize(0.0);
        filterRhoFV.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoFV.initialize(0.0);
        filterRhoFW.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoFW.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        filterRhoEU.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoEU.initialize(0.0);
        filterRhoEV.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoEV.initialize(0.0);
        filterRhoEW.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoEW.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        filterRhoRFU.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoRFU.initialize(0.0);
        filterRhoRFV.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoRFV.initialize(0.0);
        filterRhoRFW.resize(patch->getLowIndex(), patch->getHighIndex());
        filterRhoRFW.initialize(0.0);
      }
    }  

    IntVector indexLow = patch->getCellFORTLowIndex();
    IntVector indexHigh = patch->getCellFORTHighIndex();
    double start_turbTime = Time::currentSeconds();

#ifdef PetscFilter
    d_filter->applyFilter(pc, patch, rhoU, filterRhoU);
    d_filter->applyFilter(pc, patch, rhoV, filterRhoV);
    d_filter->applyFilter(pc, patch, rhoW, filterRhoW);
    d_filter->applyFilter(pc, patch, rhoUU, filterRhoUU);
    d_filter->applyFilter(pc, patch, rhoUV, filterRhoUV);
    d_filter->applyFilter(pc, patch, rhoUW, filterRhoUW);
    d_filter->applyFilter(pc, patch, rhoVV, filterRhoVV);
    d_filter->applyFilter(pc, patch, rhoVW, filterRhoVW);
    d_filter->applyFilter(pc, patch, rhoWW, filterRhoWW);
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
      d_filter->applyFilter(pc, patch, betaIJ[ii], betaHATIJ[ii]);
    }

    if (d_dynScalarModel) {
      if (d_calcScalar) {
        d_filter->applyFilter(pc, patch, rhoFU, filterRhoFU);
        d_filter->applyFilter(pc, patch, rhoFV, filterRhoFV);
        d_filter->applyFilter(pc, patch, rhoFW, filterRhoFW);
        for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
          d_filter->applyFilter(pc, patch, scalarBeta[ii], scalarBetaHat[ii]);
	}
      }
      if (d_calcEnthalpy) {
        d_filter->applyFilter(pc, patch, rhoEU, filterRhoEU);
        d_filter->applyFilter(pc, patch, rhoEV, filterRhoEV);
        d_filter->applyFilter(pc, patch, rhoEW, filterRhoEW);
        for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
          d_filter->applyFilter(pc, patch, enthalpyBeta[ii], enthalpyBetaHat[ii]);
	}
      }
      if (d_calcReactingScalar) {
        d_filter->applyFilter(pc, patch, rhoRFU, filterRhoRFU);
        d_filter->applyFilter(pc, patch, rhoRFV, filterRhoRFV);
        d_filter->applyFilter(pc, patch, rhoRFW, filterRhoRFW);
        for (int ii = 0; ii < d_lab->d_vectorMatl->size(); ii++) {
          d_filter->applyFilter(pc, patch, reactScalarBeta[ii], reactScalarBetaHat[ii]);
	}
      }
    }  

    if (pc->myrank() == 0)
      cerr << "Time for the Filter operation in Turbulence Model: " << 
	Time::currentSeconds()-start_turbTime << " seconds\n";
  TAU_PROFILE_START(compute2);
#endif
#ifdef use_fortran
    fort_comp_dynamic_2loop(cellinfo->sew,cellinfo->sns,cellinfo->stb,
	SHATIJ[0],SHATIJ[1],SHATIJ[2],SHATIJ[3],SHATIJ[4],SHATIJ[5],
	IsI, IsImag, betaHATIJ[0],betaHATIJ[1],betaHATIJ[2],
	betaHATIJ[3],betaHATIJ[4],betaHATIJ[5],
	filterRho, filterRhoU,
	filterRhoV,filterRhoW,filterRhoUU, filterRhoVV, filterRhoWW,
	filterRhoUV, filterRhoUW, filterRhoVW, 
	MLI,MMI, indexLow,indexHigh);
    if (d_dynScalarModel) {
     if (d_calcScalar) {
       fort_comp_dynamic_5loop(cellinfo->sew,cellinfo->sns,cellinfo->stb,
	SHATIJ[0],SHATIJ[1],SHATIJ[2],SHATIJ[3],SHATIJ[4],SHATIJ[5],
	filterScalarGrad[0],filterScalarGrad[1],filterScalarGrad[2],
	scalarBetaHat[0],scalarBetaHat[1],scalarBetaHat[2],
	filterRho, filterRhoU,
	filterRhoV,filterRhoW,
	filterRhoF,filterRhoFU,filterRhoFV,filterRhoFW,
	scalarNum, scalarDenom,indexLow,indexHigh);
     }
     if (d_calcEnthalpy) {
       fort_comp_dynamic_7loop(cellinfo->sew,cellinfo->sns,cellinfo->stb,
	SHATIJ[0],SHATIJ[1],SHATIJ[2],SHATIJ[3],SHATIJ[4],SHATIJ[5],
	filterEnthalpyGrad[0],filterEnthalpyGrad[1],filterEnthalpyGrad[2],
	enthalpyBetaHat[0],enthalpyBetaHat[1],enthalpyBetaHat[2],
	filterRho, filterRhoU,
	filterRhoV,filterRhoW,
	filterRhoE,filterRhoEU,filterRhoEV,filterRhoEW,
	enthalpyNum, enthalpyDenom,indexLow,indexHigh);
     }
     if (d_calcReactingScalar) {
       fort_comp_dynamic_8loop(cellinfo->sew,cellinfo->sns,cellinfo->stb,
	SHATIJ[0],SHATIJ[1],SHATIJ[2],SHATIJ[3],SHATIJ[4],SHATIJ[5],
	filterReactScalarGrad[0],filterReactScalarGrad[1],
	filterReactScalarGrad[2],
	reactScalarBetaHat[0],reactScalarBetaHat[1],reactScalarBetaHat[2],
	filterRho, filterRhoU,
	filterRhoV,filterRhoW,
	filterRhoRF,filterRhoRFU,filterRhoRFV,filterRhoRFW,
	reactScalarNum, reactScalarDenom,indexLow,indexHigh);
     }
    }
#else
    for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double delta = cellinfo->sew[colX]*
			 cellinfo->sns[colY]*cellinfo->stb[colZ];
	  double filter = pow(delta, 1.0/3.0);

	  // test filter width is assumed to be twice that of the basic filter
	  // needs following modifications:
	  // a) make the test filter work for anisotropic grid
          // b) generalize the filter operation
	  double shatij0 = (SHATIJ[0])[currCell];
	  double shatij1 = (SHATIJ[1])[currCell];
	  double shatij2 = (SHATIJ[2])[currCell];
	  double shatij3 = (SHATIJ[3])[currCell];
	  double shatij4 = (SHATIJ[4])[currCell];
	  double shatij5 = (SHATIJ[5])[currCell];
	  double IshatIcur = sqrt(2.0*(shatij0*shatij0 + shatij1*shatij1 +
				       shatij2*shatij2 + 2.0*(shatij3*shatij3 + 
				       shatij4*shatij4 + shatij5*shatij5)));
	  double filterDencur = filterRho[currCell];
//        ignoring the trace
	  double trace = (shatij0 + shatij1 + shatij2)/3.0;
//	  double trace = 0.0;

	  IsImag[currCell] = IsI[currCell]; 

	  double MIJ0cur = 2.0*filter*filter*
	                       ((betaHATIJ[0])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*(shatij0-trace));
	  double MIJ1cur = 2.0*filter*filter*
	                       ((betaHATIJ[1])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*(shatij1-trace));
	  double MIJ2cur = 2.0*filter*filter*
	                       ((betaHATIJ[2])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*(shatij2-trace));
	  double MIJ3cur = 2.0*filter*filter*
	                       ((betaHATIJ[3])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*shatij3);
	  double MIJ4cur = 2.0*filter*filter*
	                       ((betaHATIJ[4])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*shatij4);
	  double MIJ5cur =  2.0*filter*filter*
	                       ((betaHATIJ[5])[currCell]-
				2.0*2.0*filterDencur*IshatIcur*shatij5);


	  // compute Leonard stress tensor
	  // index 0: L11, 1:L22, 2:L33, 3:L12, 4:L13, 5:L23
          double filterRhoUcur = filterRhoU[currCell];
          double filterRhoVcur = filterRhoV[currCell];
          double filterRhoWcur = filterRhoW[currCell];
	  double LIJ0cur = filterRhoUU[currCell] -
			   filterRhoUcur*filterRhoUcur/filterDencur;
	  double LIJ1cur = filterRhoVV[currCell] -
			   filterRhoVcur*filterRhoVcur/filterDencur;
	  double LIJ2cur = filterRhoWW[currCell] -
			   filterRhoWcur*filterRhoWcur/filterDencur;
	  double LIJ3cur = filterRhoUV[currCell] -
			   filterRhoUcur*filterRhoVcur/filterDencur;
	  double LIJ4cur = filterRhoUW[currCell] -
			   filterRhoUcur*filterRhoWcur/filterDencur;
	  double LIJ5cur = filterRhoVW[currCell] -
			   filterRhoVcur*filterRhoWcur/filterDencur;

// Explicitly making LIJ traceless here
// Actually, trace has been ignored	  
	  double LIJtrace = (LIJ0cur + LIJ1cur + LIJ2cur)/3.0;
//	  double LIJtrace = 0.0;
	  LIJ0cur = LIJ0cur - LIJtrace;
	  LIJ1cur = LIJ1cur - LIJtrace;
	  LIJ2cur = LIJ2cur - LIJtrace;
	  (lij[0])[currCell]=LIJ0cur;
	  (lij[1])[currCell]=LIJ1cur;
	  (lij[2])[currCell]=LIJ2cur;
	  (lij[3])[currCell]=LIJ3cur;
	  (lij[4])[currCell]=LIJ4cur;
	  (lij[5])[currCell]=LIJ5cur;
	  // compute the magnitude of ML and MM
	  MLI[currCell] = MIJ0cur*LIJ0cur +
	                 MIJ1cur*LIJ1cur +
	                 MIJ2cur*LIJ2cur +
                         2.0*(MIJ3cur*LIJ3cur +
			      MIJ4cur*LIJ4cur +
			      MIJ5cur*LIJ5cur );
		// calculate absolute value of the grid strain rate
	  MMI[currCell] = MIJ0cur*MIJ0cur +
	                 MIJ1cur*MIJ1cur +
	                 MIJ2cur*MIJ2cur +
                         2.0*(MIJ3cur*MIJ3cur +
			      MIJ4cur*MIJ4cur +
			      MIJ5cur*MIJ5cur );

	  double filterRhoFcur, scalarLX, scalarLY, scalarLZ;
	  double scalarMX, scalarMY, scalarMZ;
	  double filterRhoEcur, enthalpyLX, enthalpyLY, enthalpyLZ;
	  double enthalpyMX, enthalpyMY, enthalpyMZ;
	  double filterRhoRFcur, reactScalarLX, reactScalarLY, reactScalarLZ;
	  double reactScalarMX, reactScalarMY, reactScalarMZ;
          if (d_dynScalarModel) {
            if (d_calcScalar) {
              filterRhoFcur = filterRhoF[currCell];
	      scalarLX =  filter*filter*
	                  ((scalarBetaHat[0])[currCell]-
			   2.0*2.0*filterDencur*IshatIcur*
			   (filterScalarGrad[0])[currCell]);
	      scalarLY =  filter*filter*
	                  ((scalarBetaHat[1])[currCell]-
			   2.0*2.0*filterDencur*IshatIcur*
			   (filterScalarGrad[1])[currCell]);
	      scalarLZ =  filter*filter*
	                  ((scalarBetaHat[2])[currCell]-
			   2.0*2.0*filterDencur*IshatIcur*
			   (filterScalarGrad[2])[currCell]);
	      scalarMX = filterRhoFU[currCell] -
			 filterRhoFcur*filterRhoUcur/filterDencur;
	      scalarMY = filterRhoFV[currCell] -
			 filterRhoFcur*filterRhoVcur/filterDencur;
	      scalarMZ = filterRhoFW[currCell] -
			 filterRhoFcur*filterRhoWcur/filterDencur;
	      scalarNum[currCell] = scalarLX*scalarLX +
		                    scalarLY*scalarLY +
		                    scalarLZ*scalarLZ;
	      scalarDenom[currCell] = scalarMX*scalarLX +
		                      scalarMY*scalarLY +
			              scalarMZ*scalarLZ;
            }
            if (d_calcEnthalpy) {
              filterRhoEcur = filterRhoE[currCell];
	      enthalpyLX =  filter*filter*
	                    ((enthalpyBetaHat[0])[currCell]-
			     2.0*2.0*filterDencur*IshatIcur*
			     (filterEnthalpyGrad[0])[currCell]);
	      enthalpyLY =  filter*filter*
	                    ((enthalpyBetaHat[1])[currCell]-
			     2.0*2.0*filterDencur*IshatIcur*
			     (filterEnthalpyGrad[1])[currCell]);
	      enthalpyLZ =  filter*filter*
	                    ((enthalpyBetaHat[2])[currCell]-
			     2.0*2.0*filterDencur*IshatIcur*
			     (filterEnthalpyGrad[2])[currCell]);
	      enthalpyMX = filterRhoEU[currCell] -
			   filterRhoEcur*filterRhoUcur/filterDencur;
	      enthalpyMY = filterRhoEV[currCell] -
			   filterRhoEcur*filterRhoVcur/filterDencur;
	      enthalpyMZ = filterRhoEW[currCell] -
			   filterRhoEcur*filterRhoWcur/filterDencur;
	      enthalpyNum[currCell] = enthalpyLX*enthalpyLX +
		                      enthalpyLY*enthalpyLY +
		                      enthalpyLZ*enthalpyLZ;
	      enthalpyDenom[currCell] = enthalpyMX*enthalpyLX +
		                        enthalpyMY*enthalpyLY +
			                enthalpyMZ*enthalpyLZ;
            }
            if (d_calcReactingScalar) {
              filterRhoRFcur = filterRhoRF[currCell];
	      reactScalarLX =  filter*filter*
	                       ((reactScalarBetaHat[0])[currCell]-
			        2.0*2.0*filterDencur*IshatIcur*
			        (filterReactScalarGrad[0])[currCell]);
	      reactScalarLY =  filter*filter*
	                       ((reactScalarBetaHat[1])[currCell]-
			        2.0*2.0*filterDencur*IshatIcur*
			        (filterReactScalarGrad[1])[currCell]);
	      reactScalarLZ =  filter*filter*
	                       ((reactScalarBetaHat[2])[currCell]-
			        2.0*2.0*filterDencur*IshatIcur*
			        (filterReactScalarGrad[2])[currCell]);
	      reactScalarMX = filterRhoRFU[currCell] -
			      filterRhoRFcur*filterRhoUcur/filterDencur;
	      reactScalarMY = filterRhoRFV[currCell] -
			      filterRhoRFcur*filterRhoVcur/filterDencur;
	      reactScalarMZ = filterRhoRFW[currCell] -
			      filterRhoRFcur*filterRhoWcur/filterDencur;
	      reactScalarNum[currCell] = reactScalarLX*reactScalarLX +
		                         reactScalarLY*reactScalarLY +
		                         reactScalarLZ*reactScalarLZ;
	      reactScalarDenom[currCell] = reactScalarMX*reactScalarLX +
		                           reactScalarMY*reactScalarLY +
			                   reactScalarMZ*reactScalarLZ;
            }
          }  
	}
      }
    }
#endif
  TAU_PROFILE_STOP(compute2);

  }
}




void 
CompLocalDynamicProcedure::reComputeSmagCoeff(const ProcessorGroup* pc,
				     const PatchSubset* patches,
				     const MaterialSubset*,
				     DataWarehouse* old_dw,
				     DataWarehouse* new_dw,
				     const TimeIntegratorLabel* timelabels)
{
  int nofTimeSteps=d_lab->d_sharedState->getCurrentTopLevelTimeStep();
  int initialTimeStep=10;
//  double time = d_lab->d_sharedState->getElapsedTime();
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constCCVariable<double> IsI;
    constCCVariable<double> MLI;
    constCCVariable<double> MMI;
    constCCVariable<double> scalarNum;
    constCCVariable<double> scalarDenom;
    constCCVariable<double> enthalpyNum;
    constCCVariable<double> enthalpyDenom;
    constCCVariable<double> reactScalarNum;
    constCCVariable<double> reactScalarDenom;
    CCVariable<double> Cs; //smag coeff 
    CCVariable<double> ShF; //Shmidt number 
    CCVariable<double> ShE; //Shmidt number 
    CCVariable<double> ShRF; //Shmidt number 
    constCCVariable<double> den;
    constCCVariable<double> filterRho;
    constCCVariable<double> voidFraction;
    constCCVariable<int> cellType;
    constCCVariable<double> CsOld;
    CCVariable<double> viscosity;
    CCVariable<double> scalardiff;
    CCVariable<double> enthalpydiff;
    CCVariable<double> reactScalardiff;
    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First) {
       new_dw->allocateAndPut(Cs, d_lab->d_CsLabel, matlIndex, patch);
       if (d_dynScalarModel) {
         if (d_calcScalar) {
           new_dw->allocateAndPut(ShF, d_lab->d_ShFLabel, matlIndex, patch);
         }
         if (d_calcEnthalpy) {
           new_dw->allocateAndPut(ShE, d_lab->d_ShELabel, matlIndex, patch);
         }
         if (d_calcReactingScalar) {
           new_dw->allocateAndPut(ShRF, d_lab->d_ShRFLabel, matlIndex, patch);
         }
       }      
    }
    else {
       new_dw->getModifiable(Cs, d_lab->d_CsLabel, matlIndex, patch);
       if (d_dynScalarModel) {
         if (d_calcScalar) {
           new_dw->getModifiable(ShF, d_lab->d_ShFLabel, matlIndex, patch);
         }
         if (d_calcEnthalpy) {
           new_dw->getModifiable(ShE, d_lab->d_ShELabel, matlIndex, patch);
         }
         if (d_calcReactingScalar) {
           new_dw->getModifiable(ShRF, d_lab->d_ShRFLabel, matlIndex, patch);
         }
       }      
    }
    
//    if(nofTimeSteps<=1)
//        Cs.initialize(0.0);
    if(nofTimeSteps>=initialTimeStep)
      old_dw->get(CsOld, d_lab->d_CsLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);    
    
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        ShF.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        ShE.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        ShRF.initialize(0.0);
      }
    }      

    new_dw->getModifiable(viscosity, d_lab->d_viscosityCTSLabel,
			   matlIndex, patch);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->getModifiable(scalardiff, d_lab->d_scalarDiffusivityLabel,
			      matlIndex, patch);
      }
      if (d_calcEnthalpy) {
        new_dw->getModifiable(enthalpydiff, d_lab->d_enthalpyDiffusivityLabel,
			      matlIndex, patch);
      }
      if (d_calcReactingScalar) {
        new_dw->getModifiable(reactScalardiff,
			      d_lab->d_reactScalarDiffusivityLabel,
			      matlIndex, patch);
      }
    }      

    new_dw->get(IsI,d_lab->d_strainMagnitudeLabel, matlIndex, patch, 
		Ghost::None, Arches::ZEROGHOSTCELLS);
    // using a box filter of 2*delta...will require more ghost cells if the size of filter is increased
    new_dw->get(MLI,d_lab->d_strainMagnitudeMLLabel, matlIndex, patch,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(MMI, d_lab->d_strainMagnitudeMMLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        new_dw->get(scalarNum,d_lab->d_scalarNumeratorLabel,
		    matlIndex, patch,
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(scalarDenom, d_lab->d_scalarDenominatorLabel,
		    matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcEnthalpy) {
        new_dw->get(enthalpyNum,d_lab->d_enthalpyNumeratorLabel,
		    matlIndex, patch,
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(enthalpyDenom, d_lab->d_enthalpyDenominatorLabel,
		    matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      if (d_calcReactingScalar) {
        new_dw->get(reactScalarNum,d_lab->d_reactScalarNumeratorLabel,
		    matlIndex, patch,
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
        new_dw->get(reactScalarDenom, d_lab->d_reactScalarDenominatorLabel,
		    matlIndex, patch, 
		    Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
    }      

    new_dw->get(den, d_lab->d_densityCPLabel, matlIndex, patch,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    new_dw->get(filterRho, d_lab->d_filterRhoLabel, matlIndex, patch, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);

    if (d_MAlab)
      new_dw->get(voidFraction, d_lab->d_mmgasVolFracLabel, matlIndex, patch,
		  Ghost::None, Arches::ZEROGHOSTCELLS);

    new_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);

    // Get the PerPatch CellInformation data

    PerPatch<CellInformationP> cellInfoP;
    if (new_dw->exists(d_lab->d_cellInfoLabel, matlIndex, patch)) 
      new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    else {
      cellInfoP.setData(scinew CellInformation(patch));
      new_dw->put(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    }
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    
    // get physical constants
    double viscos; // molecular viscosity
    viscos = d_physicalConsts->getMolecularViscosity();
 

    // compute test filtered velocities, density and product 
    // (den*u*u, den*u*v, den*u*w, den*v*v,
    // den*v*w, den*w*w)
    // using a box filter, generalize it to use other filters such as Gaussian
    Array3<double> MLHatI(patch->getLowIndex(), patch->getHighIndex()); // magnitude of strain rate
    MLHatI.initialize(0.0);
    Array3<double> MMHatI(patch->getLowIndex(), patch->getHighIndex()); // magnitude of test filter strain rate
    MLHatI.initialize(0.0);
    Array3<double> scalarNumHat;
    Array3<double> scalarDenomHat;
    Array3<double> enthalpyNumHat;
    Array3<double> enthalpyDenomHat;
    Array3<double> reactScalarNumHat;
    Array3<double> reactScalarDenomHat;
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        scalarNumHat.resize(patch->getLowIndex(), patch->getHighIndex());
        scalarNumHat.initialize(0.0);
        scalarDenomHat.resize(patch->getLowIndex(), patch->getHighIndex());
        scalarDenomHat.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        enthalpyNumHat.resize(patch->getLowIndex(), patch->getHighIndex());
        enthalpyNumHat.initialize(0.0);
        enthalpyDenomHat.resize(patch->getLowIndex(), patch->getHighIndex());
        enthalpyDenomHat.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        reactScalarNumHat.resize(patch->getLowIndex(), patch->getHighIndex());
        reactScalarNumHat.initialize(0.0);
        reactScalarDenomHat.resize(patch->getLowIndex(), patch->getHighIndex());
        reactScalarDenomHat.initialize(0.0);
      }
    }      
    IntVector indexLow = patch->getCellFORTLowIndex();
    IntVector indexHigh = patch->getCellFORTHighIndex();
    IntVector idxLo = patch->getGhostCellLowIndex(Arches::ONEGHOSTCELL);
    IntVector idxHi = patch->getGhostCellHighIndex(Arches::ONEGHOSTCELL);
#ifdef PetscFilter
    d_filter->applyFilter(pc, patch, MLI, MLHatI);
    d_filter->applyFilter(pc, patch, MMI, MMHatI);
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        d_filter->applyFilter(pc, patch, scalarNum, scalarNumHat);
        d_filter->applyFilter(pc, patch, scalarDenom, scalarDenomHat);
      }
      if (d_calcEnthalpy) {
        d_filter->applyFilter(pc, patch, enthalpyNum, enthalpyNumHat);
        d_filter->applyFilter(pc, patch, enthalpyDenom, enthalpyDenomHat);
      }
      if (d_calcReactingScalar) {
        d_filter->applyFilter(pc, patch, reactScalarNum, reactScalarNumHat);
        d_filter->applyFilter(pc, patch, reactScalarDenom, reactScalarDenomHat);
      }
    }      
#endif
    CCVariable<double> tempCs, Cs2;
    tempCs.allocate(patch->getLowIndex(), patch->getHighIndex());
    tempCs.initialize(0.0);
    Cs2.allocate(patch->getLowIndex(), patch->getHighIndex());
    Cs2.initialize(0.0);
    
    CCVariable<double> tempShF;
    CCVariable<double> tempShE;
    CCVariable<double> tempShRF;
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        tempShF.allocate(patch->getLowIndex(), patch->getHighIndex());
        tempShF.initialize(0.0);
      }
      if (d_calcEnthalpy) {
        tempShE.allocate(patch->getLowIndex(), patch->getHighIndex());
        tempShE.initialize(0.0);
      }
      if (d_calcReactingScalar) {
        tempShRF.allocate(patch->getLowIndex(), patch->getHighIndex());
        tempShRF.initialize(0.0);
      }
    }      
	  //     calculate the local Smagorinsky coefficient
	  //     perform "clipping" in case MLij is negative...
    for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);

	  double value;
//	  if(nofTimeSteps<initialTimeStep) {
		if ((MMHatI[currCell] < 1.0e-10)||(MLHatI[currCell] < 1.0e-7))
		  value = 0.0;
		else
		  value = MLHatI[currCell]/MMHatI[currCell];
		tempCs[currCell] = value;
	  double scalar_value;
	  double enthalpy_value;
	  double reactScalar_value;
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      if ((scalarDenomHat[currCell] < 1.0e-10)||
	          (scalarNumHat[currCell] < 1.0e-7))
	        scalar_value = 0.0;
	      else
	        scalar_value = scalarNumHat[currCell]/scalarDenomHat[currCell];
	      tempShF[currCell] = scalar_value;
            }
            if (d_calcEnthalpy) {
	      if ((enthalpyDenomHat[currCell] < 1.0e-10)||
	          (enthalpyNumHat[currCell] < 1.0e-7))
	        enthalpy_value = 0.0;
	      else
	        enthalpy_value = enthalpyNumHat[currCell]/
			         enthalpyDenomHat[currCell];
	      tempShE[currCell] = enthalpy_value;
            }
            if (d_calcReactingScalar) {
	      if ((reactScalarDenomHat[currCell] < 1.0e-10)||
	          (reactScalarNumHat[currCell] < 1.0e-7))
	        reactScalar_value = 0.0;
	      else
	        reactScalar_value = reactScalarNumHat[currCell]/
			            reactScalarDenomHat[currCell];
	      tempShRF[currCell] = reactScalar_value;
            }
          }      
	}
      }
    }
//    if (nofTimeSteps<initialTimeStep) {
      if ((d_filter_cs_squared)&&(!(d_3d_periodic))) {
      // filtering for periodic case is not implemented 
      // if it needs to be then tempCs will require 1 layer of boundary cells to be computed
#ifdef PetscFilter
        d_filter->applyFilter(pc, patch, tempCs, Cs);
        if (d_dynScalarModel) {
          if (d_calcScalar) {
            d_filter->applyFilter(pc, patch, tempShF, ShF);
          }
          if (d_calcEnthalpy) {
            d_filter->applyFilter(pc, patch, tempShE, ShE);
          }
          if (d_calcReactingScalar) {
            d_filter->applyFilter(pc, patch, tempShRF, ShRF);
          }
        }      
#endif
      }
      else
        Cs.copy(tempCs, tempCs.getLowIndex(),
		      tempCs.getHighIndex());
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
        for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
          for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
		IntVector currCell(colX, colY, colZ);
		Cs2[currCell]=Cs[currCell];
	  }
	}
      }
    
    if (d_dynScalarModel) {
      if (d_calcScalar) {
        ShF.copy(tempShF, tempShF.getLowIndex(),
		          tempShF.getHighIndex());
      }
      if (d_calcEnthalpy) {
        ShE.copy(tempShE, tempShE.getLowIndex(),
		          tempShE.getHighIndex());
      }
      if (d_calcReactingScalar) {
        ShRF.copy(tempShRF, tempShRF.getLowIndex(),
		            tempShRF.getHighIndex());
      }
    }
    
//-----------------------------------------------------------------------------------    
    if (nofTimeSteps>=initialTimeStep) {
    StencilMatrix<Array3<double> > cbetaIJ, cbetaHATIJ; //6 point tensor
    //  0-> 11, 1->22, 2->33, 3 ->12, 4->13, 5->23
    for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
      cbetaIJ[ii].resize(idxLo, idxHi);
      cbetaIJ[ii].initialize(0.0);
      cbetaHATIJ[ii].resize(idxLo, idxHi);
      cbetaHATIJ[ii].initialize(0.0);
    }  // allocate stress tensor coeffs
      StencilMatrix<constCCVariable<double> > betaIJ, LIJ; //6 point tensor
      StencilMatrix<constCCVariable<double> > SHATIJ; //6 point tensor
      for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
	  new_dw->get(betaIJ[ii], d_lab->d_betaIJCompLabel, ii, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
          new_dw->get(LIJ[ii], d_lab->d_LIJCompLabel, ii, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
          new_dw->get(SHATIJ[ii], d_lab->d_filterStrainTensorCompLabel,
		  ii, patch, Ghost::AroundCells, Arches::ONEGHOSTCELL);
      }
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
        for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
          for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double delta = cellinfo->sew[colX]*
			 cellinfo->sns[colY]*cellinfo->stb[colZ];
	  double filter = pow(delta, 1.0/3.0);

		(cbetaIJ[0])[currCell] =CsOld[currCell]*(betaIJ[0])[currCell]*filter*filter;
		(cbetaIJ[1])[currCell] =CsOld[currCell]*(betaIJ[1])[currCell]*filter*filter;
		(cbetaIJ[2])[currCell] =CsOld[currCell]*(betaIJ[2])[currCell]*filter*filter;
		(cbetaIJ[3])[currCell] =CsOld[currCell]*(betaIJ[3])[currCell]*filter*filter;
		(cbetaIJ[4])[currCell] =CsOld[currCell]*(betaIJ[4])[currCell]*filter*filter;
		(cbetaIJ[5])[currCell] =CsOld[currCell]*(betaIJ[5])[currCell]*filter*filter;
	  }
	}
      }
      for (int ii = 0; ii < d_lab->d_symTensorMatl->size(); ii++) {
#ifdef PetscFilter
          d_filter->applyFilter(pc, patch, cbetaIJ[ii], cbetaHATIJ[ii]);
#endif
      }
    
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
        for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
          for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double delta = cellinfo->sew[colX]*
			 cellinfo->sns[colY]*cellinfo->stb[colZ];
	  double filter = pow(delta, 1.0/3.0);

	  // test filter width is assumed to be twice that of the basic filter
	  // needs following modifications:
	  // a) make the test filter work for anisotropic grid
          // b) generalize the filter operation
	  double shatij0 = (SHATIJ[0])[currCell];
	  double shatij1 = (SHATIJ[1])[currCell];
	  double shatij2 = (SHATIJ[2])[currCell];
	  double shatij3 = (SHATIJ[3])[currCell];
	  double shatij4 = (SHATIJ[4])[currCell];
	  double shatij5 = (SHATIJ[5])[currCell];
	  double IshatIcur = sqrt(2.0*(shatij0*shatij0 + shatij1*shatij1 +
				       shatij2*shatij2 + 2.0*(shatij3*shatij3 + 
				       shatij4*shatij4 + shatij5*shatij5)));
	  double filterDencur = filterRho[currCell];
//        ignoring the trace
	  double trace = (shatij0 + shatij1 + shatij2)/3.0;
//	  double trace = 0.0;

	  double alphaIJ0, alphaIJ1, alphaIJ2, alphaIJ3, alphaIJ4, alphaIJ5;
	  double alphaalpha, Lalpha, cbetaHATalpha;
		alphaIJ0 = filterDencur*IshatIcur*(shatij0-trace)*filter*filter*2.0*2.0;
		alphaIJ1 = filterDencur*IshatIcur*(shatij1-trace)*filter*filter*2.0*2.0;
		alphaIJ2 = filterDencur*IshatIcur*(shatij2-trace)*filter*filter*2.0*2.0;
		alphaIJ3 = filterDencur*IshatIcur*shatij3*filter*filter*2.0*2.0;
		alphaIJ4 = filterDencur*IshatIcur*shatij4*filter*filter*2.0*2.0;
		alphaIJ5 = filterDencur*IshatIcur*shatij5*filter*filter*2.0*2.0;

		alphaalpha=alphaIJ0*alphaIJ0 + alphaIJ1*alphaIJ1 + 
			   alphaIJ2*alphaIJ2 + 2.0*(alphaIJ3*alphaIJ3 + 
			   alphaIJ4*alphaIJ4 + alphaIJ5*alphaIJ5);	
		Lalpha=alphaIJ0*(LIJ[0])[currCell] + alphaIJ1*(LIJ[1])[currCell] +
			alphaIJ2*(LIJ[2])[currCell] + 2.0*(alphaIJ3*(LIJ[3])[currCell] +
			alphaIJ4*(LIJ[4])[currCell] + alphaIJ5*(LIJ[5])[currCell]);
		cbetaHATalpha=(cbetaHATIJ[0])[currCell]*alphaIJ0 +
			(cbetaHATIJ[1])[currCell]*alphaIJ1 +
			(cbetaHATIJ[2])[currCell]*alphaIJ2 +
			2*((cbetaHATIJ[3])[currCell]*alphaIJ3 +
			(cbetaHATIJ[4])[currCell]*alphaIJ4 +
			(cbetaHATIJ[5])[currCell]*alphaIJ5); 
		tempCs[currCell]=-0.5*(Lalpha-2.0*cbetaHATalpha)/alphaalpha;
	  }
	}
      }
      if ((d_filter_cs_squared)&&(!(d_3d_periodic))) {
        // filtering for periodic case is not implemented 
        // if it needs to be then tempCs will require 1 layer of boundary cells to be computed
#ifdef PetscFilter
        d_filter->applyFilter(pc, patch, tempCs, Cs);      
#endif
      }
      else
      Cs.copy(tempCs, tempCs.getLowIndex(),
		      tempCs.getHighIndex());
    }

	  
//------------------------------------------------------------------------------------
    double factor = 1.0;
#if 0
    if (time < 2.0)
      factor = (time+0.000001)*0.5;
#endif

// Laminar Pr number is taken to be 0.7, shouldn't make much difference
    double laminarPrNo = 0.7;
    if (d_MAlab) {

      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	  for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	    IntVector currCell(colX, colY, colZ);
	    double delta = cellinfo->sew[colX]*
		           cellinfo->sns[colY]*cellinfo->stb[colZ];
	    double filter = pow(delta, 1.0/3.0);

	    Cs[currCell] = Min(Cs[currCell],10.0);
	    Cs[currCell] = factor * sqrt(Cs[currCell]);
	    viscosity[currCell] =  Cs[currCell] * Cs[currCell] *
		                   filter * filter *
	                           IsI[currCell] * den[currCell] +
				   viscos*voidFraction[currCell];
            if (nofTimeSteps>initialTimeStep) {
		    if (viscosity[currCell]<0.0)
			    viscosity[currCell]=0.0;
	    }
            if (d_dynScalarModel) {
              if (d_calcScalar) {
	        ShF[currCell] *= Cs[currCell] * Cs[currCell];
	        ShF[currCell] = Min(ShF[currCell],10.0);
	        if (ShF[currCell] >= d_lower_limit)
	          scalardiff[currCell] = Cs[currCell] * Cs[currCell] *
	                                 filter * filter *
	                                 IsI[currCell] * den[currCell] /
				         ShF[currCell] + viscos*
					 voidFraction[currCell]/laminarPrNo;
	        else
	          scalardiff[currCell] = viscos*voidFraction[currCell]/
			                 laminarPrNo;
              }
              if (d_calcEnthalpy) {
	        ShE[currCell] *= Cs[currCell] * Cs[currCell];
	        ShE[currCell] = Min(ShE[currCell],10.0);
	        if (ShE[currCell] >= d_lower_limit)
	          enthalpydiff[currCell] = Cs[currCell] * Cs[currCell] *
	                                   filter * filter *
	                                   IsI[currCell] * den[currCell] /
				           ShE[currCell] + viscos*
					   voidFraction[currCell]/laminarPrNo;
	        else
	          enthalpydiff[currCell] = viscos*voidFraction[currCell]/
			                   laminarPrNo;
              }
              if (d_calcReactingScalar) {
	        ShRF[currCell] *= Cs[currCell] * Cs[currCell];
	        ShRF[currCell] = Min(ShRF[currCell],10.0);
	        if (ShRF[currCell] >= d_lower_limit)
	          reactScalardiff[currCell] = Cs[currCell] * Cs[currCell] *
	                                      filter * filter *
	                                      IsI[currCell] * den[currCell] /
				              ShRF[currCell] +
					      viscos*voidFraction[currCell]/
					      laminarPrNo;
	        else
	          reactScalardiff[currCell] = viscos*voidFraction[currCell]/
			                      laminarPrNo;
              }
            }      
	  }
	}
      }
    }
    else {
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	  for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	    IntVector currCell(colX, colY, colZ);
	    double delta = cellinfo->sew[colX]*
		           cellinfo->sns[colY]*cellinfo->stb[colZ];
	    double filter = pow(delta, 1.0/3.0);

	    Cs[currCell] = Min(0.5*(Cs[currCell]+Cs2[currCell]),10.0);
//	    Cs[currCell] = factor * sqrt(Cs[currCell]);
	    viscosity[currCell] =  Cs[currCell] *
		                   filter * filter *
	                           IsI[currCell] * den[currCell] + viscos;
            if (nofTimeSteps>initialTimeStep) {
		    if (viscosity[currCell]<0.0)
			    viscosity[currCell]=0.0;
	    }
            if (d_dynScalarModel) {
              if (d_calcScalar) {
	        ShF[currCell] *= Cs[currCell];
	        ShF[currCell] = Min(ShF[currCell],10.0);
	        if (ShF[currCell] >= d_lower_limit)
	          scalardiff[currCell] = Cs[currCell] *
	                                 filter * filter *
	                                 IsI[currCell] * den[currCell] /
				         ShF[currCell] + viscos/laminarPrNo;
	        else
	          scalardiff[currCell] = viscos/laminarPrNo;
              }
              if (d_calcEnthalpy) {
	        ShE[currCell] *= Cs[currCell];
	        ShE[currCell] = Min(ShE[currCell],10.0);
	        if (ShE[currCell] >= d_lower_limit)
	          enthalpydiff[currCell] = Cs[currCell] *
	                                   filter * filter *
	                                   IsI[currCell] * den[currCell] /
				           ShE[currCell] + viscos/laminarPrNo;
	        else
	          enthalpydiff[currCell] = viscos/laminarPrNo;
              }
              if (d_calcReactingScalar) {
	        ShRF[currCell] *= Cs[currCell] ;
	        ShRF[currCell] = Min(ShRF[currCell],10.0);
	        if (ShRF[currCell] >= d_lower_limit)
	          reactScalardiff[currCell] = Cs[currCell] *
	                                      filter * filter *
	                                      IsI[currCell] * den[currCell] /
				              ShRF[currCell] +
					      viscos/laminarPrNo;
	        else
	          reactScalardiff[currCell] = viscos/laminarPrNo;
              }
            }      
	  }
	}
      }
    }

    // boundary conditions...make a separate function apply Boundary
    bool xminus = patch->getBCType(Patch::xminus) != Patch::Neighbor;
    bool xplus =  patch->getBCType(Patch::xplus) != Patch::Neighbor;
    bool yminus = patch->getBCType(Patch::yminus) != Patch::Neighbor;
    bool yplus =  patch->getBCType(Patch::yplus) != Patch::Neighbor;
    bool zminus = patch->getBCType(Patch::zminus) != Patch::Neighbor;
    bool zplus =  patch->getBCType(Patch::zplus) != Patch::Neighbor;
    int wall_celltypeval = d_boundaryCondition->wallCellType();
    if (xminus) {
      int colX = indexLow.x();
      for (int colZ = indexLow.z(); colZ <=  indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <=  indexHigh.y(); colY ++) {
	  IntVector currCell(colX-1, colY, colZ);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }
    if (xplus) {
      int colX =  indexHigh.x();
      for (int colZ = indexLow.z(); colZ <=  indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <=  indexHigh.y(); colY ++) {
	  IntVector currCell(colX+1, colY, colZ);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }
    if (yminus) {
      int colY = indexLow.y();
      for (int colZ = indexLow.z(); colZ <=  indexHigh.z(); colZ ++) {
	for (int colX = indexLow.x(); colX <=  indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY-1, colZ);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }
    if (yplus) {
      int colY =  indexHigh.y();
      for (int colZ = indexLow.z(); colZ <=  indexHigh.z(); colZ ++) {
	for (int colX = indexLow.x(); colX <=  indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY+1, colZ);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }
    if (zminus) {
      int colZ = indexLow.z();
      for (int colY = indexLow.y(); colY <=  indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <=  indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ-1);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }
    if (zplus) {
      int colZ =  indexHigh.z();
      for (int colY = indexLow.y(); colY <=  indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <=  indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ+1);
	  if (cellType[currCell] != wall_celltypeval) {
	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)];
//	    viscosity[currCell] = viscosity[IntVector(colX,colY,colZ)]
//		    *den[currCell]/den[IntVector(colX,colY,colZ)];
          if (d_dynScalarModel) {
            if (d_calcScalar) {
	      scalardiff[currCell] = scalardiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcEnthalpy) {
	      enthalpydiff[currCell] = enthalpydiff[IntVector(colX,colY,colZ)];
            }
            if (d_calcReactingScalar) {
	      reactScalardiff[currCell] = 
		      reactScalardiff[IntVector(colX,colY,colZ)];
            }
          }
          }	  
	}
      }
    }

  }
}


void 
CompLocalDynamicProcedure::sched_computeScalarVariance(SchedulerP& sched, 
					      const PatchSet* patches,
					      const MaterialSet* matls,
			    		 const TimeIntegratorLabel* timelabels)
{
  string taskname =  "CompLocalDynamicProcedure::computeScalarVaraince" +
		     timelabels->integrator_step_name;
  Task* tsk = scinew Task(taskname, this,
			  &CompLocalDynamicProcedure::computeScalarVariance,
			  timelabels);

  
  // Requires, only the scalar corresponding to matlindex = 0 is
  //           required. For multiple scalars this will be put in a loop
  tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);

  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);

  // Computes
  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
     tsk->computes(d_lab->d_scalarVarSPLabel);
  else
     tsk->modifies(d_lab->d_scalarVarSPLabel);

  sched->addTask(tsk, patches, matls);
}


void 
CompLocalDynamicProcedure::computeScalarVariance(const ProcessorGroup* pc,
					const PatchSubset* patches,
					const MaterialSubset*,
					DataWarehouse*,
					DataWarehouse* new_dw,
			    		const TimeIntegratorLabel* timelabels)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constCCVariable<double> scalar;
    CCVariable<double> scalarVar;
    // Get the velocity, density and viscosity from the old data warehouse
    new_dw->get(scalar, d_lab->d_scalarSPLabel, matlIndex, patch,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);

    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
    	new_dw->allocateAndPut(scalarVar, d_lab->d_scalarVarSPLabel, matlIndex,
			       patch);
    else
    	new_dw->getModifiable(scalarVar, d_lab->d_scalarVarSPLabel, matlIndex,
			       patch);
    scalarVar.initialize(0.0);

    constCCVariable<int> cellType;
    new_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    
    
    IntVector idxLo = patch->getGhostCellLowIndex(Arches::ONEGHOSTCELL);
    IntVector idxHi = patch->getGhostCellHighIndex(Arches::ONEGHOSTCELL);
    Array3<double> phiSqr(idxLo, idxHi);
    phiSqr.initialize(0.0);

    for (int colZ = idxLo.z(); colZ < idxHi.z(); colZ ++) {
      for (int colY = idxLo.y(); colY < idxHi.y(); colY ++) {
	for (int colX = idxLo.x(); colX < idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  phiSqr[currCell] = scalar[currCell]*scalar[currCell];
	}
      }
    }

    Array3<double> filterPhi(patch->getLowIndex(), patch->getHighIndex());
    Array3<double> filterPhiSqr(patch->getLowIndex(), patch->getHighIndex());
    filterPhi.initialize(0.0);
    filterPhiSqr.initialize(0.0);

    IntVector indexLow = patch->getCellFORTLowIndex();
    IntVector indexHigh = patch->getCellFORTHighIndex();

#ifdef PetscFilter
    d_filter->applyFilter(pc, patch, scalar, filterPhi);
    d_filter->applyFilter(pc, patch, phiSqr, filterPhiSqr);
#endif

    for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);

	  // compute scalar variance
	  scalarVar[currCell] = d_CFVar*(filterPhiSqr[currCell]-
					 filterPhi[currCell]*
					 filterPhi[currCell]);
	}
      }
    }

    
    // boundary conditions
    bool xminus = patch->getBCType(Patch::xminus) != Patch::Neighbor;
    bool xplus =  patch->getBCType(Patch::xplus) != Patch::Neighbor;
    bool yminus = patch->getBCType(Patch::yminus) != Patch::Neighbor;
    bool yplus =  patch->getBCType(Patch::yplus) != Patch::Neighbor;
    bool zminus = patch->getBCType(Patch::zminus) != Patch::Neighbor;
    bool zplus =  patch->getBCType(Patch::zplus) != Patch::Neighbor;
    int outlet_celltypeval = d_boundaryCondition->outletCellType();
    int pressure_celltypeval = d_boundaryCondition->pressureCellType();
    if (xminus) {
      int colX = indexLow.x();
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	  IntVector currCell(colX-1, colY, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (xplus) {
      int colX = indexHigh.x();
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	  IntVector currCell(colX+1, colY, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (yminus) {
      int colY = indexLow.y();
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY-1, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (yplus) {
      int colY = indexHigh.y();
      for (int colZ = indexLow.z(); colZ <= indexHigh.z(); colZ ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY+1, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (zminus) {
      int colZ = indexLow.z();
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ-1);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (zplus) {
      int colZ = indexHigh.z();
      for (int colY = indexLow.y(); colY <= indexHigh.y(); colY ++) {
	for (int colX = indexLow.x(); colX <= indexHigh.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ+1);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarVar[currCell] = scalarVar[IntVector(colX,colY,colZ)];
	}
      }
    }
    
  }
}
//****************************************************************************
// Schedule recomputation of the turbulence sub model 
//****************************************************************************
void 
CompLocalDynamicProcedure::sched_computeScalarDissipation(SchedulerP& sched, 
						 const PatchSet* patches,
						 const MaterialSet* matls,
			    		 const TimeIntegratorLabel* timelabels)
{
  string taskname =  "CompLocalDynamicProcedure::computeScalarDissipation" +
		     timelabels->integrator_step_name;
  Task* tsk = scinew Task(taskname, this,
			  &CompLocalDynamicProcedure::computeScalarDissipation,
			  timelabels);

  
  // Requires, only the scalar corresponding to matlindex = 0 is
  //           required. For multiple scalars this will be put in a loop
  // assuming scalar dissipation is computed before turbulent viscosity calculation 
  tsk->requires(Task::NewDW, d_lab->d_scalarSPLabel,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
  if (d_dynScalarModel)
    tsk->requires(Task::NewDW, d_lab->d_scalarDiffusivityLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
  else
    tsk->requires(Task::NewDW, d_lab->d_viscosityCTSLabel,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);

  tsk->requires(Task::NewDW, d_lab->d_cellTypeLabel, 
		Ghost::AroundCells, Arches::ONEGHOSTCELL);

  // Computes
  if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
     tsk->computes(d_lab->d_scalarDissSPLabel);
  else
     tsk->modifies(d_lab->d_scalarDissSPLabel);

  sched->addTask(tsk, patches, matls);
}


void 
CompLocalDynamicProcedure::computeScalarDissipation(const ProcessorGroup*,
					const PatchSubset* patches,
					const MaterialSubset*,
					DataWarehouse*,
					DataWarehouse* new_dw,
			    		const TimeIntegratorLabel* timelabels)
{
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int archIndex = 0; // only one arches material
    int matlIndex = d_lab->d_sharedState->getArchesMaterial(archIndex)->getDWIndex(); 
    // Variables
    constCCVariable<double> viscosity;
    constCCVariable<double> scalar;
    CCVariable<double> scalarDiss;  // dissipation..chi

    new_dw->get(scalar, d_lab->d_scalarSPLabel, matlIndex, patch,
		Ghost::AroundCells, Arches::ONEGHOSTCELL);
    if (d_dynScalarModel)
      new_dw->get(viscosity, d_lab->d_scalarDiffusivityLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    else
      new_dw->get(viscosity, d_lab->d_viscosityCTSLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);

    if (timelabels->integrator_step_number == TimeIntegratorStepNumber::First)
       new_dw->allocateAndPut(scalarDiss, d_lab->d_scalarDissSPLabel,
			      matlIndex, patch);
    else
       new_dw->getModifiable(scalarDiss, d_lab->d_scalarDissSPLabel,
			      matlIndex, patch);
    scalarDiss.initialize(0.0);

    constCCVariable<int> cellType;
    new_dw->get(cellType, d_lab->d_cellTypeLabel, matlIndex, patch,
		  Ghost::AroundCells, Arches::ONEGHOSTCELL);
    
    // Get the PerPatch CellInformation data
    PerPatch<CellInformationP> cellInfoP;
    new_dw->get(cellInfoP, d_lab->d_cellInfoLabel, matlIndex, patch);
    CellInformation* cellinfo = cellInfoP.get().get_rep();
    
    // compatible with fortran index
    IntVector idxLo = patch->getCellFORTLowIndex();
    IntVector idxHi = patch->getCellFORTHighIndex();
    for (int colZ = idxLo.z(); colZ <= idxHi.z(); colZ ++) {
      for (int colY = idxLo.y(); colY <= idxHi.y(); colY ++) {
	for (int colX = idxLo.x(); colX <= idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ);
	  double scale = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX+1,colY,colZ)]);
	  double scalw = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX-1,colY,colZ)]);
	  double scaln = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX,colY+1,colZ)]);
	  double scals = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX,colY-1,colZ)]);
	  double scalt = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX,colY,colZ+1)]);
	  double scalb = 0.5*(scalar[currCell]+
			      scalar[IntVector(colX,colY,colZ-1)]);
	  double dfdx = (scale-scalw)/cellinfo->sew[colX];
	  double dfdy = (scaln-scals)/cellinfo->sns[colY];
	  double dfdz = (scalt-scalb)/cellinfo->stb[colZ];
	  scalarDiss[currCell] = viscosity[currCell]*
	                        (dfdx*dfdx + dfdy*dfdy + dfdz*dfdz)/
				d_turbPrNo; 
	}
      }
    }

    
    // boundary conditions
    bool xminus = patch->getBCType(Patch::xminus) != Patch::Neighbor;
    bool xplus =  patch->getBCType(Patch::xplus) != Patch::Neighbor;
    bool yminus = patch->getBCType(Patch::yminus) != Patch::Neighbor;
    bool yplus =  patch->getBCType(Patch::yplus) != Patch::Neighbor;
    bool zminus = patch->getBCType(Patch::zminus) != Patch::Neighbor;
    bool zplus =  patch->getBCType(Patch::zplus) != Patch::Neighbor;
    int outlet_celltypeval = d_boundaryCondition->outletCellType();
    int pressure_celltypeval = d_boundaryCondition->pressureCellType();
    if (xminus) {
      int colX = idxLo.x();
      for (int colZ = idxLo.z(); colZ <= idxHi.z(); colZ ++) {
	for (int colY = idxLo.y(); colY <= idxHi.y(); colY ++) {
	  IntVector currCell(colX-1, colY, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (xplus) {
      int colX = idxHi.x();
      for (int colZ = idxLo.z(); colZ <= idxHi.z(); colZ ++) {
	for (int colY = idxLo.y(); colY <= idxHi.y(); colY ++) {
	  IntVector currCell(colX+1, colY, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (yminus) {
      int colY = idxLo.y();
      for (int colZ = idxLo.z(); colZ <= idxHi.z(); colZ ++) {
	for (int colX = idxLo.x(); colX <= idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY-1, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (yplus) {
      int colY = idxHi.y();
      for (int colZ = idxLo.z(); colZ <= idxHi.z(); colZ ++) {
	for (int colX = idxLo.x(); colX <= idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY+1, colZ);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (zminus) {
      int colZ = idxLo.z();
      for (int colY = idxLo.y(); colY <= idxHi.y(); colY ++) {
	for (int colX = idxLo.x(); colX <= idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ-1);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    if (zplus) {
      int colZ = idxHi.z();
      for (int colY = idxLo.y(); colY <= idxHi.y(); colY ++) {
	for (int colX = idxLo.x(); colX <= idxHi.x(); colX ++) {
	  IntVector currCell(colX, colY, colZ+1);
          if ((cellType[currCell] == outlet_celltypeval)||
            (cellType[currCell] == pressure_celltypeval))
	    if (scalar[currCell] == scalar[IntVector(colX,colY,colZ)])
	      scalarDiss[currCell] = scalarDiss[IntVector(colX,colY,colZ)];
	}
      }
    }
    
  }
}
