//static char *id="@(#) $Id$";

/*
 *  Gradient.cc:  Unfinished modules
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <SCICore/Util/NotFinished.h>
#include <PSECore/Dataflow/Module.h>
#include <PSECore/CommonDatatypes/ScalarFieldPort.h>
#include <PSECore/CommonDatatypes/SurfacePort.h>
#include <SCICore/CoreDatatypes/ScalarFieldUG.h>
#include <SCICore/CoreDatatypes/ScalarFieldRGdouble.h>
#include <SCICore/CoreDatatypes/ScalarFieldRGfloat.h>
#include <SCICore/CoreDatatypes/ScalarFieldRGint.h>
#include <SCICore/CoreDatatypes/ScalarFieldRGshort.h>
#include <SCICore/CoreDatatypes/ScalarFieldRGchar.h>
// #include <SCICore/CoreDatatypes/ScalarFieldRGuchar.h>
#include <PSECore/CommonDatatypes/VectorFieldPort.h>
#include <SCICore/CoreDatatypes/VectorFieldRG.h>
#include <SCICore/CoreDatatypes/VectorFieldUG.h>
#include <SCICore/Geometry/Point.h>
#include <SCICore/TclInterface/TCLvar.h>

namespace PSECommon {
namespace Modules {

using namespace PSECore::Dataflow;
using namespace PSECore::CommonDatatypes;
using namespace SCICore::TclInterface;
using namespace SCICore::GeomSpace;

class Gradient : public Module {
    ScalarFieldIPort* infield;
    VectorFieldOPort* outfield;
public:
    TCLint interpolate;
    Gradient(const clString& id);
    virtual ~Gradient();
    virtual void execute();
};

Module* make_Gradient(const clString& id) {
  return new Gradient(id);
}

Gradient::Gradient(const clString& id)
: Module("Gradient", id, Filter), interpolate("interpolate", id, this)
{
    infield=new ScalarFieldIPort(this, "Geometry", ScalarFieldIPort::Atomic);
    add_iport(infield);
    // Create the output port
    outfield=new VectorFieldOPort(this, "Geometry", VectorFieldIPort::Atomic);
    add_oport(outfield);
}

Gradient::~Gradient()
{
}

void Gradient::execute()
{
    ScalarFieldHandle sf;
    VectorFieldHandle vf;
    if(!infield->get(sf))
	return;
    ScalarFieldUG* sfug=sf->getUG();
    ScalarFieldRGBase* sfb=sf->getRGBase();
    if (sfug) {
	VectorFieldUG* vfug;
	if(interpolate.get()){
	    vfug=new VectorFieldUG(VectorFieldUG::NodalValues);
	    vfug->mesh=sfug->mesh;
	    vfug->data.resize(sfug->data.size());
	    Mesh* mesh=sfug->mesh.get_rep();
	    int nnodes=mesh->nodes.size();
	    Array1<Vector>& gradients=vfug->data;
	    int i;
	    for(i=0;i<nnodes;i++)
		gradients[i]=Vector(0,0,0);
	    int nelems=mesh->elems.size();
	    for(i=0;i<nelems;i++){
		if(i%1000 == 0)
		    update_progress(i, nelems);
		Element* e=mesh->elems[i];
		Point pt;
		Vector grad1, grad2, grad3, grad4;
		/*double vol=*/mesh->get_grad(e, pt, grad1, grad2, grad3, grad4);
		double v1=sfug->data[e->n[0]];
		double v2=sfug->data[e->n[1]];
		double v3=sfug->data[e->n[2]];
		double v4=sfug->data[e->n[3]];
		Vector gradient(grad1*v1+grad2*v2+grad3*v3+grad4*v4);
		for(int j=0;j<4;j++){
		    gradients[e->n[j]]+=gradient;
		}
	    }
	    for(i=0;i<nnodes;i++){
		if(i%1000 == 0)
		    update_progress(i, nnodes);
		NodeHandle& n=mesh->nodes[i];
		gradients[i]*=1./(n->elems.size());
	    }
	} else {
	    vfug=new VectorFieldUG(VectorFieldUG::ElementValues);
	    vfug->mesh=sfug->mesh;
	    Mesh* mesh=sfug->mesh.get_rep();
	    int nelems=mesh->elems.size();
	    vfug->data.resize(nelems);
	    for(int i=0;i<nelems;i++){
		//	    if(i%10000 == 0)
		//		update_progress(i, nelems);
		Element* e=mesh->elems[i];
		Point pt;
		Vector grad1, grad2, grad3, grad4;
		/*double vol=*/mesh->get_grad(e, pt, grad1, grad2, grad3, grad4);
		double v1=sfug->data[e->n[0]];
		double v2=sfug->data[e->n[1]];
		double v3=sfug->data[e->n[2]];
		double v4=sfug->data[e->n[3]];
		Vector gradient(grad1*v1+grad2*v2+grad3*v3+grad4*v4);
		vfug->data[i]=gradient;
	    }
	}
	vf=vfug;
    } else {
	int nx=sfb->nx;
	int ny=sfb->ny;
	int nz=sfb->nz;
	VectorFieldRG *vfrg=new VectorFieldRG();
	vfrg->resize(nx, ny, nz);
	Point min, max;
	sfb->get_bounds(min, max);
	vfrg->set_bounds(min, max);
	ScalarFieldRGdouble *sfrd=sfb->getRGDouble();
	ScalarFieldRGfloat *sfrf=sfb->getRGFloat();
	ScalarFieldRGint *sfri=sfb->getRGInt();
	ScalarFieldRGshort *sfrs=sfb->getRGShort();
	ScalarFieldRGchar *sfrc=sfb->getRGChar();
//	ScalarFieldRGuchar *sfru=sfb->getRGUchar();
	for(int k=0;k<nz;k++){
	    update_progress(k, nz);
	    for(int j=0;j<ny;j++){
		for(int i=0;i<nx;i++){
		    if (sfrd)
			vfrg->grid(i,j,k)=sfrd->gradient(i,j,k);
		    else if (sfrf)
			vfrg->grid(i,j,k)=sfrf->gradient(i,j,k);
		    else if (sfri)
			vfrg->grid(i,j,k)=sfri->gradient(i,j,k);
		    else if (sfrs)
			vfrg->grid(i,j,k)=sfrs->gradient(i,j,k);
		    else if (sfrc)
			vfrg->grid(i,j,k)=sfrc->gradient(i,j,k);
//		    else if (sfru)
//			vfrg->grid(i,j,k)=sfru->gradient(i,j,k);
		    else {
			cerr << "Unknown SFRG type in Gradient: "<<sfb->getType()<<".\n";
			return;
		    }
		}
	    }
	}
	vf=vfrg;
    }
    outfield->send(vf);
}

} // End namespace Modules
} // End namespace PSECommon

//
// $Log$
// Revision 1.3  1999/08/18 20:19:40  sparker
// Eliminated copy constructor and clone in all modules
// Added a private copy ctor and a private clone method to Module so
//  that future modules will not compile until they remvoe the copy ctor
//  and clone method
// Added an ASSERTFAIL macro to eliminate the "controlling expression is
//  constant" warnings.
// Eliminated other miscellaneous warnings
//
// Revision 1.2  1999/08/17 06:37:28  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:57:42  mcq
// Initial commit
//
// Revision 1.2  1999/04/28 20:51:11  dav
// deleted some files that are dependent on DaveW files
//
// Revision 1.1.1.1  1999/04/24 23:12:32  dav
// Import sources
//
//
