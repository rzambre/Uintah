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


#ifndef CORE_ALGORITHMS_MATH_BUILDFEMATRIX_H
#define CORE_ALGORITHMS_MATH_BUILDFEMATRIX_H 1

#include <Core/Algorithms/Util/DynamicAlgo.h>

#include <Core/Thread/Barrier.h>
#include <Core/Thread/Parallel.h>
#include <Core/Thread/Thread.h>

#include <Core/Geometry/Point.h>
#include <Core/Geometry/Tensor.h>
#include <Core/Basis/Locate.h>

#include <Core/Datatypes/SparseRowMatrix.h>
#include <Core/Datatypes/Matrix.h>

#include <sgi_stl_warnings_off.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sgi_stl_warnings_on.h>


namespace SCIRunAlgo {

using namespace SCIRun;

class BuildFEMatrixAlgo : public DynamicAlgoBase
{
public:
  virtual bool BuildFEMatrix(ProgressReporter *pr, FieldHandle input, MatrixHandle& output, MatrixHandle& ctable, int numproc = 1);


};

template <class FIELD>
class BuildFEMatrixAlgoT : public BuildFEMatrixAlgo
{
public:
  virtual bool BuildFEMatrix(ProgressReporter *pr, FieldHandle input, MatrixHandle& output, MatrixHandle& ctable, int numproc = 1);
  
};

template <class FIELD> class FEMBuilder;


// --------------------------------------------------------------------------
// This piece of code was adapted from BuildFEMatrix.h
// Code has been modernized a little to meet demands.

template <class FIELD>
class FEMBuilder : public DynamicAlgoBase
{
public:

  // Constructor needed as Barrier needs to have name
  FEMBuilder() :
    barrier_("FEMBuilder Barrier")
  {
  }

  // Local entry function for none pure function.
  void build_matrix(FieldHandle input, MatrixHandle& output, MatrixHandle& ctable, int numproc);

private:

  // For parallel implementation
  Barrier barrier_;

  typename FIELD::mesh_type::basis_type fieldbasis_;
  typename FIELD::mesh_handle_type meshhandle_;

  FieldHandle meshfield_;
  FIELD *fieldptr_;

  MatrixHandle fematrixhandle_;
  SparseRowMatrix* fematrix_;

  int numprocessors_;
  int* rows_;
  int* allcols_;
  std::vector<int> colidx_;

  int domain_dimension;

  int local_dimension_nodes;
  int local_dimension_add_nodes;
  int local_dimension_derivatives;
  int local_dimension;

  int global_dimension_nodes;
  int global_dimension_add_nodes;
  int global_dimension_derivatives;
  int global_dimension; 

  // A copy of the tensors list that was generated by SetConductivities
  std::vector<std::pair<string, Tensor> > tensors_;

  // Entry point for the parallel version
  void parallel(int proc);
    
private:
  
  // General case where we can indexed or non indexed data
  template<class T>
  inline Tensor get_tensor(T& val) const
  {
    if (tensors_.size() == 0) return(Tensor(static_cast<double>(val)));
    return (tensors_[static_cast<size_t>(val)].second);
  }

  // Specific case for when we have a tensor as datatype
  inline Tensor get_tensor(const Tensor& val) const
  {
    return (val);
  }

  inline void add_lcl_gbl(int row, const std::vector<int> &cols, const std::vector<double> &lcl_a)
  {
    for (int i = 0; i < (int)lcl_a.size(); i++) fematrix_->add(row, cols[i], lcl_a[i]);
  }

private:

  void create_numerical_integration(std::vector<std::vector<double> > &p,std::vector<double> &w,std::vector<std::vector<double> > &d);
  void build_local_matrix(typename FIELD::mesh_type::Elem::index_type c_ind,int row, std::vector<double> &l_stiff,std::vector<std::vector<double> > &p,std::vector<double> &w,std::vector<std::vector<double> >  &d);
  void setup();
  
};



template <class FIELD>
bool BuildFEMatrixAlgoT<FIELD>::BuildFEMatrix(ProgressReporter *pr, FieldHandle input, MatrixHandle& output, MatrixHandle& ctable, int numproc)
{
  // Some sanity checks
  FIELD* field = dynamic_cast<FIELD *>(input.get_rep());
  if (field == 0)
  {
    pr->error("BuildFEMatrix: Could not obtain input field");
    return (false);
  }

  if (ctable.get_rep())
  {
    if ((ctable->ncols() != 1)&&(ctable->ncols() != 6)&&(ctable->ncols() != 9))
    {
      pr->error("BuildFEMatrix: Conductivity table needs to have 1, 6, or 9 columns");
      return (false);
    } 
    if (ctable->nrows() == 0)
    { 
      pr->error("BuildFEMatrix: ConductivityTable is empty");
      return (false);
    }
  }

  // We build a second class to do the building of the FEMatrix so we can use
  // class variables to use in the multi threaded applications. This way the 
  // original main class is pure and we can use one instance to do multiple
  // computations. This helps when precomputing dynamic algorithms

  Handle<FEMBuilder<FIELD> > builder = scinew FEMBuilder<FIELD>;
  // Call the the none pure version
  builder->build_matrix(input,output,ctable,numproc);

  if (output.get_rep() == 0)
  {    
    pr->error("BuildFEMatrix: Could not build output matrix");
    return (false);
  }
  
  return (true);
}


template <class FIELD>
void FEMBuilder<FIELD>::build_matrix(FieldHandle input, MatrixHandle& output, MatrixHandle& ctable, int numproc)
{
  // Retrieve useful properly typed pointers from the input data
  fieldptr_ = dynamic_cast<FIELD *>(input.get_rep());
  meshhandle_ = fieldptr_->get_typed_mesh();

  // Determine the number of processors to use:
  int np = Thread::numProcessors();
  if (np > 5) np = 5;  // Never do more than 5 processors automatically
  
  // Get the number of processors from the user input
  // If they specify a number use it if 0 ignore it
  if (numproc > 0) { np = numproc; }
  numprocessors_ = np;
  
  // If we have the Conductivity property use it, if not we assume the values on
  // the data to be the actual tensors.
  fieldptr_->get_property("conductivity_table",tensors_);
  
  // We added a second system of adding a conductivity table, using a matrix
  // Convert that matrix in the conducivity table
  if (ctable.get_rep())
  {
    DenseMatrix* mat = ctable->dense();
    // Only if we can convert it into a denso matrix, otherwise skip it
    if (mat)
    {
      double* data = ctable->get_data_pointer();
      int m = ctable->nrows();
      Tensor T; 

      // Case the table has isotropic conductivities
      if (mat->ncols() == 1)
      {
        for (int p=0; p<m;p++)
        {
          // Set the diagonals to the proper version.
          T.mat_[0][0] = data[0*m+p];
          T.mat_[1][0] = 0.0;
          T.mat_[2][0] = 0.0;
          T.mat_[0][1] = 0.0;
          T.mat_[1][1] = data[0*m+p];
          T.mat_[2][1] = 0.0;
          T.mat_[0][2] = 0.0;
          T.mat_[1][2] = 0.0;
          T.mat_[2][2] = data[0*m+p];
          tensors_.push_back(std::pair<string, Tensor>("",T));
        }
      }
       
      // Use our compressed way of storing tensors 
      if (mat->ncols() == 6)
      {
        for (int p=0; p<m;p++)
        {
          T.mat_[0][0] = data[0*m+p];
          T.mat_[1][0] = data[1*m+p];
          T.mat_[2][0] = data[2*m+p];
          T.mat_[0][1] = data[1*m+p];
          T.mat_[1][1] = data[3*m+p];
          T.mat_[2][1] = data[4*m+p];
          T.mat_[0][2] = data[2*m+p];
          T.mat_[1][2] = data[4*m+p];
          T.mat_[2][2] = data[5*m+p];
          tensors_.push_back(std::pair<string, Tensor>("",T));
        }
      }

      // Use the full symmetric tensor. We will make the tensor symmetric here.
      if (mat->ncols() == 9)
      {
        for (int p=0; p<m;p++)
        {
          T.mat_[0][0] = data[0*m+p];
          T.mat_[1][0] = data[1*m+p];
          T.mat_[2][0] = data[2*m+p];
          T.mat_[0][1] = data[1*m+p];
          T.mat_[1][1] = data[4*m+p];
          T.mat_[2][1] = data[5*m+p];
          T.mat_[0][2] = data[2*m+p];
          T.mat_[1][2] = data[5*m+p];
          T.mat_[2][2] = data[8*m+p];
          tensors_.push_back(std::pair<string, Tensor>("",T));
        }
      }
    }
  }
  
  // Start the multi threaded FE matrix builder.
  Thread::parallel(this, &FEMBuilder<FIELD>::parallel, numprocessors_);

  // Copy the output back to the output handle reference
  output = fematrixhandle_;
}






template <class FIELD>
void FEMBuilder<FIELD>::create_numerical_integration(std::vector<std::vector<double> > &p,
                                                   std::vector<double> &w,
                                                   std::vector<std::vector<double> > &d)
{
  p.resize(fieldbasis_.GaussianNum);
  w.resize(fieldbasis_.GaussianNum);
  d.resize(fieldbasis_.GaussianNum);

  for(int i=0; i < fieldbasis_.GaussianNum; i++)
  {
    w[i] = fieldbasis_.GaussianWeights[i];

    p[i].resize(domain_dimension);
    for(int j=0; j<domain_dimension; j++)
      p[i][j]=fieldbasis_.GaussianPoints[i][j];

    d[i].resize(local_dimension*3);
    fieldbasis_.get_derivate_weights(p[i], (double *)&d[i][0]);
  }
}


//! build line of the local stiffness matrix
template <class FIELD>
void FEMBuilder<FIELD>::build_local_matrix(typename FIELD::mesh_type::Elem::index_type c_ind,
                                            int row, std::vector<double> &l_stiff,
                                            std::vector<std::vector<double> > &p,
                                            std::vector<double> &w,
                                            std::vector<std::vector<double> >  &d)
{
  Tensor T = get_tensor(fieldptr_->value(c_ind));

  double Ca = T.mat_[0][0];
  double Cb = T.mat_[0][1];
  double Cc = T.mat_[0][2];
  double Cd = T.mat_[1][1];
  double Ce = T.mat_[1][2];
  double Cf = T.mat_[2][2];


  for(int i=0; i<local_dimension; i++)
    l_stiff[i] = 0.0;

  int local_dimension2=2*local_dimension;

  double vol;
  const int dim = meshhandle_->dimensionality();
  if (dim == 3)
  {
    vol = meshhandle_->get_basis().volume();  
  }
  else if (dim == 2)
  {
    // We need to get the proper factor, a surface has zero volume, so we need to
    // get the area. The Jacobian is made in such a way that its determinant
    // is a area ratio factor 
    vol = meshhandle_->get_basis().area(0);
  }
  else
  {
    // We need to get the proper factor, a curve has zero volume, so we need to
    // get the length. The Jacobian is made in such a way that its determinant
    // is a length ratio factor 
    vol = meshhandle_->get_basis().length(0);  
  }

  for (unsigned int i = 0; i < d.size(); i++)
  {
    std::vector<Point> Jv;
    meshhandle_->derivate(p[i],c_ind,Jv);

    double J[9], Ji[9];

    if (dim == 3)
    {
      J[0] = Jv[0].x();
      J[1] = Jv[0].y();
      J[2] = Jv[0].z();
      J[3] = Jv[1].x();
      J[4] = Jv[1].y();
      J[5] = Jv[1].z();
      J[6] = Jv[2].x();
      J[7] = Jv[2].y();
      J[8] = Jv[2].z();    

    }
    else if (dim == 2)
    {
      // For the surface elements there is no third derivative, there are only
      // two. Hence we cannot compute the Jacobian. However, actually in real space
      // the element is defined in 3 dimensions, hence it needs to be in the 
      // warped space of the element definition. In this case it is only defined in
      // two dimensions, but we can add a third. The only thing is that in this 
      // direction we don't want any lengthing or shortening. Hence it should be a
      // unit vector. This makes the determinant of the Jacobian as well the area
      // Volume = Area*1. Hence we can still use the scheme.

      // Store third one in separate vector, so we do not need to do a resize,
      // which is costly.
   
      // This should make the deterimant always positive, in a 3D world one cannot
      // define the proper clockwise order of a surface element as one does not
      // know what is inside and what is outside
      Vector J2;
      J2 = Cross(Jv[0].asVector(),Jv[1].asVector());
      J2.normalize();
      J[0] = Jv[0].x();
      J[1] = Jv[0].y();
      J[2] = Jv[0].z();
      J[3] = Jv[1].x();
      J[4] = Jv[1].y();
      J[5] = Jv[1].z();
      J[6] = J2.x();
      J[7] = J2.y();
      J[8] = J2.z();    
    }
    else if (dim == 1)
    {
      // The same thing as for the surface but then for a curve.
      // Again this matrix should have a positive determinant as well. It actually
      // has an internal degree of freedom, which is not being used.
      Vector J1, J2;
      Jv[0].asVector().find_orthogonal(J1,J2);
      J[0] = Jv[0].x();
      J[1] = Jv[0].y();
      J[2] = Jv[0].z();
      J[3] = J1.x();
      J[4] = J1.y();
      J[5] = J1.z();
      J[6] = J2.x();
      J[7] = J2.y();
      J[8] = J2.z();          
    }
    
    // Compute the inverse Jacobian and the determinant, the latter is the volume
    // ratio between real element and the element in unit space
    double detJ = InverseMatrix3x3(J, Ji);
               
    // Volume elements can return negative determinants if the order of elements
    // is put in a different order
    // TODO: It seems to be that a negative determinant is not necessarily bad, 
    // we should be more flexible on thiis point
    ASSERT(detJ>0);
    
    // Volume associated with the local Gaussian Quadrature point:
    // weightfactor * Volume Unit element * Volume ratio (real element/unit element)
    detJ*=w[i]*vol;
	
    // Build local stiffness matrix
    // Get the local derivatives of the basis functions in the basis element
    // They are all the same and are thus precomputed in matrix d
    const double *Nxi = &d[i][0];
    const double *Nyi = &d[i][local_dimension];
    const double *Nzi = &d[i][local_dimension2];
    // Gradients associated with the node we are calculating
    const double &Nxip = Nxi[row];
    const double &Nyip = Nyi[row];
    const double &Nzip = Nzi[row];
    // Calculating gradient shape function * inverse Jacobian * volume scaling factor
    const double uxp = detJ*(Nxip*Ji[0]+Nyip*Ji[1]+Nzip*Ji[2]);
    const double uyp = detJ*(Nxip*Ji[3]+Nyip*Ji[4]+Nzip*Ji[5]);
    const double uzp = detJ*(Nxip*Ji[6]+Nyip*Ji[7]+Nzip*Ji[8]);
    // Matrix multiplication with conductivity tensor :
    const double uxyzpabc = uxp*Ca+uyp*Cb+uzp*Cc;
    const double uxyzpbde = uxp*Cb+uyp*Cd+uzp*Ce;
    const double uxyzpcef = uxp*Cc+uyp*Ce+uzp*Cf;
	
    // The above is constant for this node. Now multiply with the weight function
    // We assume the weight factors are the same as the local gradients 
    // Galerkin approximation:
     
    for (unsigned int j = 0; j<local_dimension; j++)
    {
      const double &Nxj = Nxi[j];
      const double &Nyj = Nyi[j];
      const double &Nzj = Nzi[j];
	
      // Matrix multiplication Gradient with inverse Jacobian:
      const double ux = Nxj*Ji[0]+Nyj*Ji[1]+Nzj*Ji[2];
      const double uy = Nxj*Ji[3]+Nyj*Ji[4]+Nzj*Ji[5];
      const double uz = Nxj*Ji[6]+Nyj*Ji[7]+Nzj*Ji[8];
      
      // Add everything together into one coeffiecient of the matrix
      l_stiff[j] += ux*uxyzpabc+uy*uxyzpbde+uz*uxyzpcef;
    }
  }

}

template <class FIELD>
void FEMBuilder<FIELD>::setup()
{
  // The domain dimension
  domain_dimension = fieldbasis_.domain_dimension();
  ASSERT(domain_dimension>0);

  local_dimension_nodes = fieldbasis_.number_of_mesh_vertices();
  local_dimension_add_nodes = fieldbasis_.number_of_vertices()-fieldbasis_.number_of_mesh_vertices();
  local_dimension_derivatives = 0;
  
  // Local degrees of freedom per element
  local_dimension = local_dimension_nodes + local_dimension_add_nodes + local_dimension_derivatives; //!< degrees of freedom (dofs) of system
  ASSERT(fieldbasis_.dofs()==local_dimension);

  typename FIELD::mesh_type::Node::size_type mns;
  meshhandle_->size(mns);
  // Number of mesh points (not necessarily number of nodes)
  global_dimension_nodes = mns;
  global_dimension_add_nodes = fieldptr_->get_basis().size_node_values();
  global_dimension_derivatives = fieldptr_->get_basis().size_derivatives();
  global_dimension = global_dimension_nodes+global_dimension_add_nodes+global_dimension_derivatives;

  meshhandle_->synchronize(Mesh::EDGES_E | Mesh::NODE_NEIGHBORS_E);
  rows_ = scinew int[global_dimension+1];
  
  colidx_.resize(numprocessors_+1);
}


// -- callback routine to execute in parallel
template <class FIELD>
void FEMBuilder<FIELD>::parallel(int proc_num)
{
  if (proc_num == 0)
  {
    setup();
  }
  
  barrier_.wait(numprocessors_);

  //! distributing dofs among processors
  const int start_gd = global_dimension * proc_num/numprocessors_;
  const int end_gd  = global_dimension * (proc_num+1)/numprocessors_;

  //! creating sparse matrix structure
  std::vector<unsigned int> mycols;
  mycols.reserve((end_gd - start_gd)*local_dimension*8);  //<! rough estimate

  typename FIELD::mesh_type::Elem::array_type ca;
  typename FIELD::mesh_type::Node::array_type na;
  typename FIELD::mesh_type::Edge::array_type ea;
  std::vector<int> neib_dofs;

  //! loop over system dofs for this thread
  for (unsigned int i = start_gd; i<end_gd; i++)
  {
    rows_[i] = mycols.size();

    neib_dofs.clear();
    //! check for nodes
    if (i<global_dimension_nodes)
    {
      //! get neighboring cells for node
      typename FIELD::mesh_type::Node::index_type idx;
      meshhandle_->to_index(idx,i);
      meshhandle_->get_elems(ca, idx);
    }
    else if (i<global_dimension_nodes+global_dimension_add_nodes)
    {
      //! check for additional nodes at edges
      //! get neighboring cells for node
      const int ii = i-global_dimension_nodes;
      typename FIELD::mesh_type::Edge::index_type idx;
      typename FIELD::mesh_type::Node::array_type nodes;
      typename FIELD::mesh_type::Elem::array_type elems;
      typename FIELD::mesh_type::Elem::array_type elems2;

      meshhandle_->to_index(idx,ii);
      meshhandle_->get_nodes(nodes,idx);
      meshhandle_->get_elems(elems,nodes[0]);
      meshhandle_->get_elems(elems2,nodes[1]);
      ca.clear();
      for (int v=0; v < elems.size(); v++)
         for (int w=0; w <elems2.size(); w++)
            if (elems[v] == elems2[w]) ca.push_back(elems[v]);
    }
    else
    {
      //! check for derivatives - to do
    }
	
    for(unsigned int j = 0; j < ca.size(); j++)
    {
      //! get neighboring nodes
      meshhandle_->get_nodes(na, ca[j]);

      for(unsigned int k = 0; k < na.size(); k++) 
      {
        neib_dofs.push_back((int)(na[k])); // Must cast to (int) for SGI compiler. :-(
      }

      //! check for additional nodes at edges
      if (global_dimension_add_nodes)
      {
        //! get neighboring edges
        meshhandle_->get_edges(ea, ca[j]);

        for(unsigned int k = 0; k < ea.size(); k++)
          neib_dofs.push_back(global_dimension + ea[k]);
      }
    }
	
    std::sort(neib_dofs.begin(), neib_dofs.end());

    for (unsigned int j=0; j<neib_dofs.size(); j++)
    {
      if (j == 0 || neib_dofs[j] != (int)mycols.back())
      {
        mycols.push_back(neib_dofs[j]);
      }
    }
  }

  colidx_[proc_num] = mycols.size();

  //! check point
  barrier_.wait(numprocessors_);

  int st = 0;
  if (proc_num == 0)
  {
    for(int i=0; i<numprocessors_; i++)
    {
      const int ns = colidx_[i];
      colidx_[i] = st;
      st += ns;
    }

    colidx_[numprocessors_] = st;
    allcols_ = scinew int[st];
  }

  //! check point
  barrier_.wait(numprocessors_);

  //! updating global column by each of the processors
  const int s = colidx_[proc_num];
  const int n = mycols.size();

  for(int i=0; i<n; i++)
    allcols_[i+s] = mycols[i];

  for(int i = start_gd; i<end_gd; i++)
    rows_[i] += s;

  //! check point
  barrier_.wait(numprocessors_);

  //! the main thread makes the matrix
  if (proc_num == 0)
  {
    rows_[global_dimension] = st;
    double* vals_ = scinew double[st];
    for (int p=0; p <st; p++) vals_[p] = 0.0;
    fematrix_ = scinew SparseRowMatrix(global_dimension, global_dimension, rows_, allcols_, st,vals_);
    fematrixhandle_ = fematrix_;
  }

  //! check point
  barrier_.wait(numprocessors_);

  //! zeroing in parallel
  const int ns = colidx_[proc_num];
  const int ne = colidx_[proc_num+1];
  double* a = &fematrix_->a[ns], *ae=&fematrix_->a[ne];

  while (a<ae) *a++=0.0;

  std::vector<std::vector<double> > ni_points;
  std::vector<double> ni_weights;
  std::vector<std::vector<double> > ni_derivatives;
  create_numerical_integration(ni_points, ni_weights, ni_derivatives);

  std::vector<double> lsml; //!< line of local stiffnes matrix
  lsml.resize(local_dimension);
      	
  //! loop over system dofs for this thread
  for (int i = start_gd; i<end_gd; i++)
  {
    if (i < global_dimension_nodes)
    {
      //! check for nodes
      //! get neighboring cells for node
      typename FIELD::mesh_type::Node::index_type idx;
      meshhandle_->to_index(idx,i);
      meshhandle_->get_elems(ca,idx);
    }
    else if (i < global_dimension_nodes + global_dimension_add_nodes)
    {
      //! check for additional nodes at edges
      //! get neighboring cells for additional nodes
      const int ii=i-global_dimension_nodes;
      typename FIELD::mesh_type::Edge::index_type idx;
      typename FIELD::mesh_type::Node::array_type nodes;
      typename FIELD::mesh_type::Elem::array_type elems;
      typename FIELD::mesh_type::Elem::array_type elems2;

      meshhandle_->to_index(idx,ii);
      meshhandle_->get_nodes(nodes,idx);
      meshhandle_->get_elems(elems,nodes[0]);
      meshhandle_->get_elems(elems2,nodes[1]);
      ca.clear();
      for (int v=0; v < elems.size(); v++)
         for (int w=0; w <elems2.size(); w++)
            if (elems[v] == elems2[w]) ca.push_back(elems[v]);

    }
    else
    {
      //! check for derivatives - to do
    }
	
    //! loop over elements attributed elements
    for (int j = 0; j < (int)ca.size(); j++)
    {
      neib_dofs.clear();
      meshhandle_->get_nodes(na, ca[j]); //!< get neighboring nodes
      for(int k = 0; k < (int)na.size(); k++)
      {
        neib_dofs.push_back((int)(na[k])); // Must cast to (int) for SGI compiler :-(
      }
      //! check for additional nodes at edges
      if (global_dimension_add_nodes)
      {
        meshhandle_->get_edges(ea, ca[j]); //!< get neighboring edges
        for(int k = 0; k < (int)ea.size(); k++)
        {
          neib_dofs.push_back(global_dimension + ea[k]);
        }
      }
      
      ASSERT((int)neib_dofs.size() == local_dimension);
      for(int k = 0; k < (int)na.size(); k++)
      {
        if ((int)na[k] == i) 
        {
          build_local_matrix(ca[j], k , lsml, ni_points, ni_weights, ni_derivatives);
          add_lcl_gbl(i, neib_dofs, lsml);
        }
      }

      if (global_dimension_add_nodes)
      {
        for(int k = 0; k < (int)ea.size(); k++)
        {
          if (global_dimension + ea[k] == i)
          {
            build_local_matrix(ca[j], k+(int)na.size() , lsml, ni_points, ni_weights, ni_derivatives);
            add_lcl_gbl(i, neib_dofs, lsml);
          }
        }
      }
      
    }
  }

  
  barrier_.wait(numprocessors_);
}

} // end namespace SCIRun

#endif 
