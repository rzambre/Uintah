#ifndef UINTAH_HOMEBREW_DataWarehouse_H
#define UINTAH_HOMEBREW_DataWarehouse_H


#include <Uintah/Grid/Handle.h>
#include <Uintah/Grid/GridP.h>
#include <Uintah/Grid/CCVariable.h>
#include <Uintah/Grid/DataItem.h>
#include <Uintah/Grid/RefCounted.h>
#include <Uintah/Grid/ParticleVariableBase.h>
#include <Uintah/Grid/NCVariableBase.h>
#include <Uintah/Grid/ReductionVariableBase.h>
#include <Uintah/Interface/DataWarehouseP.h>

namespace SCICore {
namespace Geometry {
  class Vector;
}
}

namespace Uintah {
   class VarLabel;
/**************************************
	
CLASS
   DataWarehouse
	
   Short description...
	
GENERAL INFORMATION
	
   DataWarehouse.h
	
   Steven G. Parker
   Department of Computer Science
   University of Utah
	
   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
	
   Copyright (C) 2000 SCI Group
	
KEYWORDS
   DataWarehouse
	
DESCRIPTION
   Long description...
	
WARNING
	
****************************************/
      
   class DataWarehouse : public RefCounted {
   public:
      virtual ~DataWarehouse();
      
      DataWarehouseP getTop() const;
      
      virtual void setGrid(const GridP&)=0;
      
      virtual void allocate(ReductionVariableBase&, const VarLabel*) = 0;
      virtual void get(ReductionVariableBase&, const VarLabel*) const = 0;
      virtual void put(const ReductionVariableBase&, const VarLabel*) = 0;
      
      virtual void allocate(int numParticles, ParticleVariableBase&,
			    const VarLabel*, int matlIndex,
			    const Region*) = 0;
      virtual void allocate(ParticleVariableBase&, const VarLabel*,	int matlIndex,
			    const Region*) = 0;
      virtual void get(ParticleVariableBase&, const VarLabel*,
		       int matlIndex, const Region*, int numGhostCells) const = 0;
      virtual void put(const ParticleVariableBase&, const VarLabel*,
		       int matlIndex, const Region*) = 0;
      
      virtual void allocate(NCVariableBase&, const VarLabel*,
			    int matlIndex, const Region*) = 0;
      virtual void get(NCVariableBase&, const VarLabel*,
		       int matlIndex, const Region*, int numGhostCells) const = 0;
      virtual void put(const NCVariableBase&, const VarLabel*,
		       int matlIndex, const Region*) = 0;
      
   protected:
      DataWarehouse( int MpiRank, int MpiProcesses );
      int d_MpiRank, d_MpiProcesses;
      
   private:
      
      DataWarehouse(const DataWarehouse&);
      DataWarehouse& operator=(const DataWarehouse&);
   };
   
} // end namespace Uintah

//
// $Log$
// Revision 1.16  2000/04/28 07:35:39  sparker
// Started implementation of DataWarehouse
// MPM particle initialization now works
//
// Revision 1.15  2000/04/27 23:18:51  sparker
// Added problem initialization for MPM
//
// Revision 1.14  2000/04/26 06:49:10  sparker
// Streamlined namespaces
//
// Revision 1.13  2000/04/25 00:41:22  dav
// more changes to fix compilations
//
// Revision 1.12  2000/04/24 15:17:02  sparker
// Fixed unresolved symbols
//
// Revision 1.11  2000/04/21 20:31:25  dav
// added some allocates
//
// Revision 1.10  2000/04/20 18:56:35  sparker
// Updates to MPM
//
// Revision 1.9  2000/04/19 21:20:04  dav
// more MPI stuff
//
// Revision 1.8  2000/04/19 05:26:17  sparker
// Implemented new problemSetup/initialization phases
// Simplified DataWarehouse interface (not finished yet)
// Made MPM get through problemSetup, but still not finished
//
// Revision 1.7  2000/04/13 06:51:05  sparker
// More implementation to get this to work
//
// Revision 1.6  2000/04/11 07:10:53  sparker
// Completing initialization and problem setup
// Finishing Exception modifications
//
// Revision 1.5  2000/03/22 00:37:17  sparker
// Added accessor for PerRegion data
//
// Revision 1.4  2000/03/17 18:45:43  dav
// fixed a few more namespace problems
//
// Revision 1.3  2000/03/16 22:08:22  dav
// Added the beginnings of cocoon docs.  Added namespaces.  Did a few other coding standards updates too
//
//

#endif
