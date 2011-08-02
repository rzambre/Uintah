#ifndef QMOM_Expr_h
#define QMOM_Expr_h
#include <spatialops/structured/FVStaggeredFieldTypes.h>
#include <spatialops/structured/FVStaggeredOperatorTypes.h>

#include <expression/Expr_Expression.h>
#include <math.h>

// declare lapack eigenvalue solver
extern "C"{
  void dsyev_( char* jobz, char* uplo, int* n, double* a, int* lda,
        double* w, double* work, int* lwork, int* info );
}


/**
 *  \class QMOM
 */
template<typename FieldT>
class QMOM : public Expr::Expression<FieldT>
{
  typedef std::vector<const FieldT*> FieldTVec;
  FieldTVec knownMoments_;  
  const Expr::TagList knownMomentsTagList_;
  QMOM(const Expr::TagList knownMomentsTagList,
       const Expr::ExpressionID& id,
       const Expr::ExpressionRegistry& reg);

public:
  class Builder : public Expr::ExpressionBuilder
  {
  public:
    Builder( const Expr::TagList knownMomentsTagList )
    : knownmomentstaglist_(knownMomentsTagList)
    {}
    
    Expr::ExpressionBase*
    build( const Expr::ExpressionID& id,
          const Expr::ExpressionRegistry& reg ) const {
      return new QMOM<FieldT>(knownmomentstaglist_, id, reg);
    }

  private:
    const Expr::TagList knownmomentstaglist_;
  };

  ~QMOM();

  void advertise_dependents( Expr::ExprDeps& exprDeps );

  void bind_fields( const Expr::FieldManagerList& fml );

  void bind_operators( const SpatialOps::OperatorDatabase& opDB );

  void evaluate();

};


// ###################################################################
//
//                          Implementation
//
// ###################################################################


template<typename FieldT>
QMOM<FieldT>::
QMOM( const Expr::TagList knownMomentsTaglist,
       const Expr::ExpressionID& id,
       const Expr::ExpressionRegistry& reg  )
  : Expr::Expression<FieldT>(id,reg),
knownMomentsTagList_(knownMomentsTaglist)
{
}

//--------------------------------------------------------------------
template<typename FieldT>
QMOM<FieldT>::
~QMOM()
{}

//--------------------------------------------------------------------
template< typename FieldT >
void
QMOM<FieldT>::
advertise_dependents( Expr::ExprDeps& exprDeps )
{
  exprDeps.requires_expression( knownMomentsTagList_ );
}

//--------------------------------------------------------------------

template< typename FieldT >
void
QMOM<FieldT>::
bind_fields( const Expr::FieldManagerList& fml )
{
  const Expr::FieldManager<FieldT>& fm = fml.field_manager<FieldT>();
  /* add additional code here to bind any fields required by this expression */
  // iterate over taglist
  knownMoments_.clear();
  for (Expr::TagList::const_iterator iMomTag=knownMomentsTagList_.begin(); iMomTag!=knownMomentsTagList_.end(); ++iMomTag)
      knownMoments_.push_back(&fm.field_ref(*iMomTag));
      
}

//--------------------------------------------------------------------

template< typename FieldT >
void
QMOM<FieldT>::
bind_operators( const SpatialOps::OperatorDatabase& opDB )
{
  // bind operators as follows:
  // op_ = opDB.retrieve_operator<OpT>();
}

//--------------------------------------------------------------------

template< typename FieldT >
void
QMOM<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  using namespace SCIRun;
  // the results vector is organized as follows:
  // (w0, a0, w1, a1, et...) 
  typedef std::vector<FieldT*> ResultsVec;
  ResultsVec& results = this->get_value_vec();
  //
  const int nMoments = knownMoments_.size();
  double **p = new double *[nMoments];
  for (int i = 0; i<nMoments; i++)
    p[i] = new double [nMoments+1];
  p[0][0] = 1.0;
  //
  const int abSize = nMoments/2;
  double* alpha = new double[nMoments];
  double* a = new double[abSize];
  double* b = new double[abSize];
  double* jMatrix = new double[abSize*abSize];
  double* eigenValues = new double[abSize];
  double* weights = new double[abSize];  
  // loop over every point in the patch. get a sample iterator for any of the
  // fields.
  const FieldT* sampleField = knownMoments_[0];
  typename FieldT::const_interior_iterator sampleIterator = sampleField->begin();
  double m0;
  //
  // create vector of iterators for the known moments and for the results
  //
  std::vector<typename FieldT::const_interior_iterator> knownMomentsIterators;
  std::vector<typename FieldT::interior_iterator> resultsIterators;
  for (int i=0; i<nMoments; i++) {
    typename FieldT::const_interior_iterator thisIterator = knownMoments_[i]->begin();
    knownMomentsIterators.push_back(thisIterator);
    
    typename FieldT::interior_iterator thisResultsIterator = results[i]->begin();
    resultsIterators.push_back(thisResultsIterator);
  }
  //
  while (sampleIterator!=sampleField->end()) {        
    // for every point, calculate the quadrature weights and abscissae
    // start by putting together the p matrix. this is documented in an associated pdf
    for (int iRow=0; iRow<=nMoments-2; iRow += 2) {
      // get the the iRow moment for this point
      p[iRow][1] = *knownMomentsIterators[iRow];
      // get the the (iRow+1) moment for all points      
      p[iRow+1][1] = -*knownMomentsIterators[iRow+1];
    }
    // keep filling the p matrix. this gets easier now
    for (int jCol=2; jCol<=nMoments; jCol++) {
      for (int iRow=0; iRow <= nMoments - jCol; iRow++) {
        p[iRow][jCol] = p[0][jCol-1]*p[iRow+1][jCol-2] - p[0][jCol-2]*p[iRow+1][jCol-1];
      }
    }

//    for (int iRow=0; iRow<nMoments; iRow++) {
//      for (int jCol=0; jCol<nMoments+1; jCol++) {
//        printf("p[%i][%i] = %f \t \t",iRow,jCol,p[iRow][jCol]);
//      }
//      printf("\n");
//    }

    //
    //
    alpha[0]=0.0;
    for (int jCol=1; jCol<nMoments; jCol++) 
      alpha[jCol] = p[0][jCol+1]/(p[0][jCol]*p[0][jCol-1]);      

//    for (int iRow=0; iRow<nMoments; iRow++) {
//        printf("alpha[%i] = %f \t \t",iRow,alpha[iRow]);
//    }
//    printf("\n");

    //_________________________
    // construct a and b arrays
    for (int jCol=0; jCol < abSize-1; jCol++) {
      const int twojcol = 2*jCol;
      a[jCol] = alpha[twojcol+1] + alpha[twojcol];
      const double rhsB = alpha[twojcol+1]*alpha[twojcol+2];
      //
      if (rhsB < 0) {
        std::ostringstream errorMsg;
        errorMsg 	<< endl
                  << "ERROR: Negative number detected in constructing the b auxiliary matrix while processing the QMOM expression." << std::endl
					        << "Value: b["<<jCol<<"] = "<<rhsB << std::endl;
        throw std::runtime_error( errorMsg.str() );        
      }
      b[jCol] = -sqrt(rhsB);
    }
    // fill in the last entry for a
    a[abSize-1] = alpha[2*(abSize-1) + 1] + alpha[2*(abSize-1)];
//    for (int iRow=0; iRow<abSize; iRow++) {
//      printf("a[%i] = %f \t \t",iRow,a[iRow]);
//    }
//    printf("\n");
//    for (int iRow=0; iRow<abSize; iRow++) {
//      printf("b[%i] = %f \t \t",iRow,b[iRow]);
//    }
//    printf("\n");
    //___________________
    // construct J matrix
    for (int iRow=0; iRow<abSize - 1; iRow++) {
      jMatrix[iRow+iRow*abSize] = a[iRow];
      jMatrix[iRow+(iRow+1)*abSize] = b[iRow];
      jMatrix[iRow + 1 + iRow*abSize] = b[iRow];
    }
    jMatrix[abSize*abSize-1] = a[abSize - 1];
//    for (int iRow=0; iRow<abSize*abSize; iRow++) {
//      printf("J[%i] = %f \t \t",iRow,jMatrix[iRow]);
//    }
//    printf("\n");

    //__________
    // Eigenvalue solve
    /* Query and allocate the optimal workspace */    
    int n = abSize, lda = abSize, info, lwork;
    double wkopt;
    double* work;    
    lwork = -1;
    char jobz='V';
    char matType = 'U';
    dsyev_( &jobz, &matType, &n, jMatrix, &lda, eigenValues, &wkopt, &lwork, &info );
    lwork = (int)wkopt;
    work = new double[lwork];
    // Solve eigenproblem. eigenvectors are stored in the jMatrix, columnwise
    dsyev_( &jobz,&matType, &n, jMatrix, &lda, eigenValues, work, &lwork, &info );
    delete[] work;
//    for (int iRow=0; iRow<abSize; iRow++) {
//      printf("Eigen[%i] = %.12f \t \t",iRow,eigenValues[iRow]);
//    }
//    printf("\n");    
    //__________
    // save the weights and abscissae
    m0 = *knownMomentsIterators[0];
    for (int i=0; i<abSize; i++) {
      int matLoc = i*abSize;
      weights[i] = jMatrix[matLoc]*jMatrix[matLoc]/m0;
    }    
    for (int i=0; i<abSize; i++) {
      int matLoc = 2*i;
      *resultsIterators[matLoc] = weights[i];
      *resultsIterators[matLoc + 1] = eigenValues[i];
    }
//
//    for (int i=0; i<abSize; i++) {
//      int matLoc = 2*i;
//      printf("w[%i] = %.12f ",i,*resultsIterators[matLoc]);
//      printf("a[%i] = %.12f ",i,*resultsIterators[matLoc+1]);      
//    }
//    printf("\n");
        
    //__________
    // increment iterators
    ++sampleIterator;
    for (int i=0; i<nMoments; i++) {
      knownMomentsIterators[i] += 1;
      resultsIterators[i] += 1;
    }    
  }
  delete[] a;
  delete[] b;
  delete[] alpha;
  delete[] eigenValues;
  delete[] weights;
  delete[] jMatrix;
  for (int i = 0; i<nMoments; i++) delete[] p[i];
  delete[] p;
}

#endif // QMOM_Expr_h
