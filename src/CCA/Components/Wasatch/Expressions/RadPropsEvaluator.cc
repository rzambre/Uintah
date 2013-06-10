/**
 *  \file   RadPropsEvaluator.cc
 *  \date   Jun 6, 2013
 *  \author "James C. Sutherland"
 *
 *
 * The MIT License
 *
 * Copyright (c) 2013 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "RadPropsEvaluator.h"


template< typename FieldT >
RadPropsEvaluator<FieldT>::
RadPropsEvaluator( const Expr::Tag& tempTag,
                   const RadSpecMap& species,
                   const std::string& fileName )
  : Expr::Expression<FieldT>(),
    tempTag_( tempTag ),
    greyGas_( new GreyGas(fileName) )
{
  // obtain the list of species from the file:
  const std::vector<RadiativeSpecies>& specOrder = greyGas_->speciesGG();
  BOOST_FOREACH( const RadiativeSpecies& sp, specOrder ){
    const RadSpecMap::const_iterator isp = species.find(sp);
    if( isp == species.end() ){
      std::ostringstream msg;
      msg << __FILE__ << " : " << __LINE__ << std::endl
          << "Species '" << species_name(sp) << "' was found in file " << fileName
          << " but no corresponding species was provided in the input file." << std::endl;
      throw std::invalid_argument( msg.str() );
    }
    else{
      indepVarNames_.push_back( isp->second );
    }
  }
}

//--------------------------------------------------------------------

template< typename FieldT >
RadPropsEvaluator<FieldT>::
~RadPropsEvaluator()
{
  delete greyGas_;
}

//--------------------------------------------------------------------

template< typename FieldT >
void
RadPropsEvaluator<FieldT>::
advertise_dependents( Expr::ExprDeps& exprDeps )
{
  BOOST_FOREACH( const Expr::Tag& tag, indepVarNames_ ){
    exprDeps.requires_expression( tag );
  }
  exprDeps.requires_expression( tempTag_ );
}

//--------------------------------------------------------------------

template< typename FieldT >
void
RadPropsEvaluator<FieldT>::
bind_fields( const Expr::FieldManagerList& fml )
{
  const typename Expr::FieldMgrSelector<FieldT>::type& fm = fml.template field_manager<FieldT>();

  indepVars_.clear();
  BOOST_FOREACH( const Expr::Tag& tag, indepVarNames_ ){
    indepVars_.push_back( &fm.field_ref(tag) );
  }
  temp_ = &fm.field_ref( tempTag_ );
}

//--------------------------------------------------------------------

template< typename FieldT >
void
RadPropsEvaluator<FieldT>::
evaluate()
{
  FieldT& result = this->value();

  typedef typename FieldT::const_iterator Iterator;
  typedef std::vector<Iterator> IVarIter;

  IVarIter ivarIters;
  for( typename IndepVarVec::const_iterator i=indepVars_.begin(); i!=indepVars_.end(); ++i ){
    ivarIters.push_back( (*i)->begin() );
  }

  std::vector<double> ivarsPoint( indepVars_.size(), 0.0 );

  // loop over grid points.  iii is a dummy variable.
  Iterator itemp=temp_->begin();
  for( typename FieldT::iterator iprop=result.begin(); iprop!=result.end(); ++iprop, ++itemp ){

    // extract indep vars at this grid point
    ivarsPoint.clear();
    for( typename IVarIter::const_iterator i=ivarIters.begin(); i!=ivarIters.end(); ++i ){
      ivarsPoint.push_back( **i );
    }

    // for now, we only pull out the effective radiative coefficient
    double planck=0, rosseland=0;
    greyGas_->mixture_coeffs( planck, rosseland, *iprop, ivarsPoint, *itemp );
  }
}

//--------------------------------------------------------------------

template< typename FieldT >
RadPropsEvaluator<FieldT>::
Builder::Builder( const Expr::Tag& resultTag,
                  const Expr::Tag& tempTag,
                  const RadSpecMap& species,
                  const std::string& fileName  )
  : ExpressionBuilder( resultTag ),
    tempTag_( tempTag ),
    rsm_( species ),
    fileName_( fileName )
{}

//--------------------------------------------------------------------

template< typename FieldT >
Expr::ExpressionBase*
RadPropsEvaluator<FieldT>::
Builder::build() const
{
  return new RadPropsEvaluator<FieldT>( tempTag_, rsm_, fileName_ );
}


//==========================================================================
// Explicit template instantiation for supported versions of this expression
#include <spatialops/structured/FVStaggered.h>
template class RadPropsEvaluator<SpatialOps::structured::SVolField>;
//==========================================================================
