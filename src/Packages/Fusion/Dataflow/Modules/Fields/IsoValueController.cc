/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


/*
 *  IsoValueController.cc: Send a serries of Isovalues to the IsoSurface module
 *                         and collect the resulting surfaces.
 *
 *  Written by:
 *   Allen R. Sanderson
 *   University of Utah
 *   August 2004
 *
 *  Copyright (C) 2004 SCI Group
 */

#include <Core/Datatypes/ColumnMatrix.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Dataflow/Ports/FieldPort.h>

#include <Core/Datatypes/FieldInterface.h>

#include <Core/Datatypes/TriSurfField.h>
#include <Core/Datatypes/QuadSurfField.h>
#include <Core/Datatypes/CurveField.h>

#include <Core/Containers/StringUtil.h>


namespace Fusion {

using namespace SCIRun;

class PSECORESHARE IsoValueController : public Module {
public:
  IsoValueController(GuiContext*);

  virtual ~IsoValueController();

  virtual void execute();

  virtual void tcl_command(GuiArgs&, void*);

private:
  GuiString IsoValueStr_;

  int nvalues_;
  vector< double > isovalues_;

  double prev_min_;
  double prev_max_;
  int last_generation_;
 
  MatrixHandle mHandleIsoValue_;
  MatrixHandle mHandleIndex_;

  FieldHandle fHandle_;
  FieldHandle fHandle_2D_;
  FieldHandle fHandle_3D_;
  GeomHandle  gHandle_;

  bool error_;
};


DECLARE_MAKER(IsoValueController)
IsoValueController::IsoValueController(GuiContext* context)
  : Module("IsoValueController", context, Source, "Fields", "Fusion"),
    IsoValueStr_(context->subVar("isovalues")),
    prev_min_(0),
    prev_max_(0),
    last_generation_(-1),
    error_(false)
{
  ColumnMatrix *isovalueMtx = scinew ColumnMatrix(1);
  mHandleIsoValue_ = MatrixHandle(isovalueMtx);
  
  ColumnMatrix *indexMtx = scinew ColumnMatrix(2);
  mHandleIndex_ = MatrixHandle(indexMtx);
}

IsoValueController::~IsoValueController(){
}

void IsoValueController::execute() {

  FieldIPort *ifp = (FieldIPort *)get_iport("Input Field");
  if (!ifp) {
    error("Unable to initialize iport 'Input Field'.");
    return;
  }

  // Get a handle to the input 2D field port.
  FieldIPort *ifield2D_port = (FieldIPort *)get_iport("2D Field");
  if (!ifield2D_port) {
    error("Unable to initialize oport '2D Field'.");
    return;
  }

  // Get a handle to the input 3D field port.
  FieldIPort *ifield3D_port = (FieldIPort *)get_iport("3D Field");
  if (!ifield3D_port) {
    error("Unable to initialize oport '3D Field'.");
    return;
  }

  // Get a handle to the input geometery port.
  GeometryIPort *igeometry_port = (GeometryIPort *)get_iport("Axis Geometry");
  if (!igeometry_port) {
    error("Unable to initialize oport 'Axis Geometry'.");
    return;
  }

  // Get a handle to the output isovalue port.
  MatrixOPort *omatrixIsovalue_port = (MatrixOPort *)get_oport("Isovalue");
  if (!omatrixIsovalue_port) {
    error("Unable to initialize oport 'Isovalue'.");
    return;
  }

  // Get a handle to the output index port.
  MatrixOPort *omatrixIndex_port = (MatrixOPort *)get_oport("Index");
  if (!omatrixIndex_port) {
    error("Unable to initialize oport 'Index'.");
    return;
  }

  // Get the current field.
  if (!(ifp->get(fHandle_) && fHandle_.get_rep())) {
    error( "No field handle or representation." );
    return;
  }

  bool update = false;

  if ( fHandle_->generation != last_generation_ ) {
    // new field
    ScalarFieldInterfaceHandle sfi = fHandle_->query_scalar_interface(this);
    if (!sfi.get_rep()) {
      error("Input field does not contain scalar data.");
      return;
    }
    
    // Set min/max
    pair<double, double> minmax;
    sfi->compute_min_max(minmax.first, minmax.second);
    if (minmax.first != prev_min_ || minmax.second != prev_max_) {
      prev_min_ = minmax.first;
      prev_max_ = minmax.second;
    }

    last_generation_ = fHandle_->generation;
    update = true;
  }

  vector< double > isovalues(0);

  istringstream vlist(IsoValueStr_.get());
  double val;
  while(!vlist.eof()) {
    vlist >> val;
    if (vlist.fail()) {
      if (!vlist.eof()) {
	vlist.clear();
	warning("List of Isovals was bad at character " +
		to_string((int)(vlist.tellg())) +
		"('" + ((char)(vlist.peek())) + "').");
      }
      break;
    }
    else if (!vlist.eof() && vlist.peek() == '%') {
      vlist.get();
      val = prev_min_ + (prev_max_ - prev_min_) * val / 100.0;
    }

    if( prev_min_ <= val && val <= prev_max_ )
      isovalues.push_back(val);
    else {
      error("Isovalue is less than the minimum or is greater than the maximum.");
      ostringstream str;
      str << "Minimum: " << prev_min_ << " Maximum: " << prev_max_;
    
      error( str.str());
      return;
    }
  }

  if( isovalues_.size() != isovalues.size() ){
    update = true;

    isovalues_.resize(isovalues.size());

    for( unsigned int i=0; i<isovalues.size(); i++ )
      isovalues_[i] == isovalues[i];

  } else {
    for( unsigned int i=0; i<isovalues.size(); i++ )
      if( fabs( isovalues_[i] - isovalues[i] ) > 1.0e-4 ) {
	update = true;
	break;
      }
  }

  vector< FieldHandle > fHandles_2D;
  vector< FieldHandle > fHandles_3D;
  vector< GeomHandle  > gHandles;

  if( update || error_ ) {
    error_ = false;

    unsigned int ncols = (int) sqrt( (double) isovalues.size() );

    if( ncols * ncols < isovalues.size() )
      ncols++;

    unsigned int nrows = isovalues.size() / ncols;

    if( nrows * ncols < isovalues.size() )
      nrows++;

    int row = 0;
    unsigned int col = 0;

    for( unsigned int i=0; i<isovalues.size(); i++ ) {

      mHandleIsoValue_.get_rep()->put(0, 0, isovalues[i]);

      mHandleIndex_.get_rep()->put(0, 0, (double) col);
      mHandleIndex_.get_rep()->put(1, 0, (double) row);

      ostringstream str;
      str << "Using Isovalue " << isovalues[i];
      str << "  Row " << row << " Col " << col;
    
      if( ++col == ncols ) {
	col = 0;
	--row;
      }

      if( i<isovalues.size()-1 ) {
	omatrixIsovalue_port->send_intermediate(mHandleIsoValue_);
	omatrixIndex_port->send_intermediate(mHandleIndex_);
      } else {
	omatrixIsovalue_port->send(mHandleIsoValue_);
	omatrixIndex_port->send(mHandleIndex_);
	str << "  Done";
      }

      remark( str.str());

      // Now get the isosurfaces.
      FieldHandle fHandle;
           
      if (!(ifield2D_port->get(fHandle) && fHandle.get_rep())) {
	error( "No 2D field handle or representation." );
	error_ = true;
	return;
      } else
	fHandles_2D.push_back( fHandle );

      if (!(ifield3D_port->get(fHandle) && fHandle.get_rep())) {
	error( "No 3D field handle or representation." );
	error_ = true;
	return;
      } else
	fHandles_3D.push_back( fHandle );

      GeomHandle geometryin;
      /*           
      if (!(igeometry_port->getObj(geometryin) && geometryin.get_rep())) {
	error( "No geometry handle or representation." );
	return;
      } else
	gHandles.push_back( geometryin );
      */
    }
  }


  // Output field.
  if (fHandles_2D.size() && fHandles_2D[0].get_rep()) {

    FieldOPort *ofield2D_port = (FieldOPort *)get_oport("2D Fields");
    if (!ofield2D_port) {
      error("Unable to initialize oport '2D Fields'.");
      return;
    }

    FieldOPort *ofield3D_port = (FieldOPort *)get_oport("3D Fields");
    if (!ofield3D_port) {
      error("Unable to initialize oport '3D Fields'.");
      return;
    }

    GeometryOPort *ogeometry_port = (GeometryOPort *)get_oport("Axis Geometry");
    if (!ogeometry_port) {
      error("Unable to initialize oport 'Axis Geometry'.");
      return;
    }

    // Copy the name of field to the downstream field.
    string fldname;
    if (!fHandle_->get_property("name",fldname))
      fldname = string("Isosurface");

    for (unsigned int i=0; i<fHandles_2D.size(); i++) {
      fHandles_2D[i]->set_property("name",fldname, false);
      fHandles_3D[i]->set_property("name",fldname, false);
    }

    // Single field.
    if (fHandles_2D.size() == 1) {
      ofield2D_port->send(fHandles_2D[0]);
      ofield3D_port->send(fHandles_3D[0]);

    // Multiple fields.
    } else {
      const TypeDescription *mtd = fHandles_2D[0]->get_type_description(0);
      
      if( mtd->get_name() == "TriSurfField" ) {
	vector<TriSurfField<double> *> tfields_2D(fHandles_2D.size());
	vector<TriSurfField<double> *> tfields_3D(fHandles_3D.size());
	for (unsigned int i=0; i<fHandles_2D.size(); i++) {
	  tfields_2D[i] = (TriSurfField<double> *)(fHandles_2D[i].get_rep());
	  tfields_3D[i] = (TriSurfField<double> *)(fHandles_3D[i].get_rep());
	}

	ofield2D_port->send(append_fields(tfields_2D));
	ofield3D_port->send(append_fields(tfields_3D));

      } else if( mtd->get_name() == "CurveField" ) {

	vector<CurveField<double> *> cfields_2D(fHandles_2D.size());
	vector<CurveField<double> *> cfields_3D(fHandles_3D.size());
	for (unsigned int i=0; i<fHandles_2D.size(); i++) {
	  cfields_2D[i] = (CurveField<double> *)(fHandles_2D[i].get_rep());
	  cfields_3D[i] = (CurveField<double> *)(fHandles_3D[i].get_rep());
	}

	ofield2D_port->send(append_fields(cfields_2D));
	ofield3D_port->send(append_fields(cfields_3D));

      } else if( mtd->get_name() == "QuadSurfField" ) {

	vector<QuadSurfField<double> *> qfields_2D(fHandles_2D.size());
	vector<QuadSurfField<double> *> qfields_3D(fHandles_3D.size());
	for (unsigned int i=0; i<fHandles_2D.size(); i++) {
	  qfields_2D[i] = (QuadSurfField<double> *)(fHandles_2D[i].get_rep());
	  qfields_3D[i] = (QuadSurfField<double> *)(fHandles_3D[i].get_rep());
	}

	ofield2D_port->send(append_fields(qfields_2D));
	ofield3D_port->send(append_fields(qfields_3D));

      } else {
	warning("Unable to append field: " + mtd->get_name() );
	ofield2D_port->send(fHandles_2D[0]);
	ofield3D_port->send(fHandles_3D[0]);
      }
    }

    for (unsigned int i=0; i<gHandles.size(); i++)
      ogeometry_port->addObj(gHandles[i],fldname);

    if (gHandles.size())
      ogeometry_port->flushViews();
  }
}

void
IsoValueController::tcl_command(GuiArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}

} // End namespace Fusion
