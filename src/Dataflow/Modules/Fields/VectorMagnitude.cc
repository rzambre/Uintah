/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  VectorMagnitude.cc:  Unfinished modules
 *
 *  Written by:
 *   Michael Callahan
 *   Department of Computer Science
 *   University of Utah
 *   March 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/FieldPort.h>

#include <Dataflow/Modules/Fields/VectorMagnitude.h>

namespace SCIRun {

class VectorMagnitude : public Module
{
public:
  VectorMagnitude(GuiContext* ctx);
  virtual ~VectorMagnitude();

  virtual void execute();

protected:
  FieldHandle fieldout_;

  int fGeneration_;
};


DECLARE_MAKER(VectorMagnitude)

VectorMagnitude::VectorMagnitude(GuiContext* ctx)
  : Module("VectorMagnitude", ctx, Filter, "Fields", "SCIRun"),
    fGeneration_(-1)
{
}

VectorMagnitude::~VectorMagnitude()
{
}

void
VectorMagnitude::execute()
{
  FieldIPort* ifp = (FieldIPort *)get_iport("Input Field");

  FieldHandle fieldin;
  Field *field;

  if (!ifp) {
    error( "Unable to initialize "+name+"'s iport" );
    return;
  }

  if (!(ifp->get(fieldin) && (field = fieldin.get_rep())))
  {
    error( "No handle or representation" );
    return;
  }

  // If no data or a changed recalcute.
  if( !fieldout_.get_rep() ||
      fGeneration_ != fieldin->generation ) {
    fGeneration_ = fieldin->generation;

    const TypeDescription *ftd = fieldin->get_type_description(0);

    CompileInfo *ci = VectorMagnitudeAlgo::get_compile_info(ftd);
    DynamicAlgoHandle algo_handle;
    if (! DynamicLoader::scirun_loader().get(*ci, algo_handle)) {
      error( "Could not compile algorithm." );
      return;
    }
    VectorMagnitudeAlgo *algo =
      dynamic_cast<VectorMagnitudeAlgo *>(algo_handle.get_rep());
    if (algo == 0) {
      error( "Could not get algorithm." );
      return;
    }

    fieldout_ = algo->execute(fieldin);

    if( fieldout_.get_rep() == NULL ) {
      error( "Only availible for Vector data." );
      return;
    }
  }

  // Get a handle to the output field port.
  if( fieldout_.get_rep() ) {
    FieldOPort* ofp = (FieldOPort *) get_oport("Output VectorMagnitude");

    if (!ofp) {
      error("Unable to initialize "+name+"'s oport\n");
      return;
    }

    // Send the data downstream
    ofp->send(fieldout_);
  }
}

CompileInfo *
VectorMagnitudeAlgo::get_compile_info(const TypeDescription *field_td)
{
  // use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const string include_path(TypeDescription::cc_to_h(__FILE__));
  static const string template_class_name("VectorMagnitudeAlgoT");
  static const string base_class_name("VectorMagnitudeAlgo");

  CompileInfo *rval = 
    scinew CompileInfo(template_class_name + "." +
		       field_td->get_filename() + ".",
                       base_class_name, 
                       template_class_name, 
                       field_td->get_name());

  // Add in the include path to compile this obj
  rval->add_include(include_path);
  field_td->fill_compile_info(rval);
  return rval;
}

} // End namespace SCIRun


