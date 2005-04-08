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

#include <Core/Persistent/PersistentSTL.h>
#include <Core/Geometry/Tensor.h>
#include <Core/Geometry/Vector.h>
#include <Core/Datatypes/GenericField.h>
#include <Core/Basis/HexTrilinearLgn.h>
#include <Core/Containers/FData.h>
#include <Core/Datatypes/LatVolMesh.h>
#include <Core/Datatypes/MaskedLatVolMesh.h>
#include <Core/Datatypes/MRLatVolField.h>

//#include <Core/Datatypes/MaskedLatVolField.h>


#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
/*
cc-1468 CC: REMARK File = ../src/Core/Datatypes/cd_templates_fields_0.cc, Line = 11
  Inline function "SCIRun::FData3d<SCIRun::Tensor>::end" cannot be explicitly
          instantiated.
*/
#pragma set woff 1468
#endif

using namespace SCIRun;
typedef LatVolMesh<HexTrilinearLgn<Point> > LVMesh;
typedef HexTrilinearLgn<Tensor>             FDTensorBasis;
typedef HexTrilinearLgn<Vector>             FDVectorBasis;
typedef HexTrilinearLgn<double>             FDdoubleBasis;
typedef HexTrilinearLgn<float>              FDfloatBasis;
typedef HexTrilinearLgn<int>                FDintBasis;
typedef HexTrilinearLgn<short>              FDshortBasis;
typedef HexTrilinearLgn<char>               FDcharBasis;
typedef HexTrilinearLgn<unsigned int>       FDuintBasis;
typedef HexTrilinearLgn<unsigned short>     FDushortBasis;
typedef HexTrilinearLgn<unsigned char>      FDucharBasis;
typedef HexTrilinearLgn<unsigned long>      FDulongBasis;

template class GenericField<LVMesh, FDTensorBasis,  FData3d<Tensor, LVMesh> >;
template class GenericField<LVMesh, FDVectorBasis,  FData3d<Vector, LVMesh> >;
template class GenericField<LVMesh, FDdoubleBasis,  FData3d<double, LVMesh> >;
template class GenericField<LVMesh, FDfloatBasis,   FData3d<float, LVMesh> >;
template class GenericField<LVMesh, FDintBasis,     FData3d<int, LVMesh> >;
template class GenericField<LVMesh, FDshortBasis,   FData3d<short, LVMesh> >;
template class GenericField<LVMesh, FDcharBasis,    FData3d<char, LVMesh> >;
template class GenericField<LVMesh, FDuintBasis,    
			    FData3d<unsigned int, LVMesh> >;
template class GenericField<LVMesh, FDushortBasis,  
			    FData3d<unsigned short, LVMesh> >;
template class GenericField<LVMesh, FDucharBasis,   
			    FData3d<unsigned char, LVMesh> >;
template class GenericField<LVMesh, FDulongBasis,   
			    FData3d<unsigned long, LVMesh> >;

typedef MaskedLatVolMesh<HexTrilinearLgn<Point> > MLVMesh;

template class GenericField<MLVMesh, FDTensorBasis, FData3d<Tensor, MLVMesh> >;
template class GenericField<MLVMesh, FDVectorBasis, FData3d<Vector, MLVMesh> >;
template class GenericField<MLVMesh, FDdoubleBasis, FData3d<double, MLVMesh> >;
template class GenericField<MLVMesh, FDfloatBasis,  FData3d<float, MLVMesh> >;
template class GenericField<MLVMesh, FDintBasis,    FData3d<int, MLVMesh> >;
template class GenericField<MLVMesh, FDshortBasis,  FData3d<short, MLVMesh> >;
template class GenericField<MLVMesh, FDcharBasis,   FData3d<char, MLVMesh> >;
template class GenericField<MLVMesh, FDuintBasis,   
			    FData3d<unsigned int, MLVMesh> >;
template class GenericField<MLVMesh, FDushortBasis,  
			    FData3d<unsigned short, MLVMesh> >;
template class GenericField<MLVMesh, FDucharBasis,   
			    FData3d<unsigned char, MLVMesh> >;
template class GenericField<MLVMesh, FDulongBasis,   
			    FData3d<unsigned long, MLVMesh> >;


template class MRLatVolField<Tensor>;
template class MRLatVolField<Vector>;
template class MRLatVolField<double>;
template class MRLatVolField<float>;
template class MRLatVolField<int>;
template class MRLatVolField<short>;
template class MRLatVolField<char>;
template class MRLatVolField<unsigned int>;
template class MRLatVolField<unsigned short>;
template class MRLatVolField<unsigned char>;

const TypeDescription* get_type_description(MRLatVolField<Tensor> *);
const TypeDescription* get_type_description(MRLatVolField<Vector> *);
const TypeDescription* get_type_description(MRLatVolField<double> *);
const TypeDescription* get_type_description(MRLatVolField<float> *);
const TypeDescription* get_type_description(MRLatVolField<int> *);
const TypeDescription* get_type_description(MRLatVolField<short> *);
const TypeDescription* get_type_description(MRLatVolField<char> *);
const TypeDescription* get_type_description(MRLatVolField<unsigned int> *);
const TypeDescription* get_type_description(MRLatVolField<unsigned short> *);
const TypeDescription* get_type_description(MRLatVolField<unsigned char> *);




#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1468
#endif
