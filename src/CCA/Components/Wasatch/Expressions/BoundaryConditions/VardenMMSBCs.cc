/*
 * The MIT License
 *
 * Copyright (c) 2012 The University of Utah
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
 */

#include "VardenMMSBCs.h"
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

// ###################################################################
//
//                          Implementation
//
// ###################################################################

template< typename FieldT >
void
VarDensMMSDensity<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  FieldT& f = this->value();
  const double ci = this->ci_;
  const double cg = this->cg_;
  const double t = (*t_)[0];

  const double bcValue = -1 / ( (5/(exp(1125/( t + 10)) * (2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125 / (t + 10)) * (2 * t + 5)));
  if ( (this->vecGhostPts_) && (this->vecInteriorPts_) ) {
    std::vector<SpatialOps::structured::IntVec>::const_iterator ig = (this->vecGhostPts_)->begin();    // ig is the ghost flat index
    std::vector<SpatialOps::structured::IntVec>::const_iterator ii = (this->vecInteriorPts_)->begin(); // ii is the interior flat index
    for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
      f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
    }
  }
}

// ###################################################################
//
//                          Implementation
//
// ###################################################################

template< typename FieldT >
void
VarDensMMSMixtureFraction<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  FieldT& f = this->value();
  const double ci = this->ci_;
  const double cg = this->cg_;
  
  const double t = (*t_)[0];  // this breaks GPU.
  
  if ( (this->vecGhostPts_) && (this->vecInteriorPts_) ) {
    std::vector<SpatialOps::structured::IntVec>::const_iterator ig = (this->vecGhostPts_)->begin();    // ig is the ghost flat index
    std::vector<SpatialOps::structured::IntVec>::const_iterator ii = (this->vecInteriorPts_)->begin(); // ii is the interior flat index
    const double bcValue = ( (5. / (2. * t +5.)) * exp(-1125 / (10. + t)) );
    for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
      f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
    }
  }
}

// ###################################################################
//
//                          Implementation
//
// ###################################################################

template< typename FieldT >
void
VarDensMMSMomentum<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  namespace SS = SpatialOps::structured;
  FieldT& f = this->value();
  const double ci = this->ci_;
  const double cg = this->cg_;
  const double t = (*t_)[0];
  
  if ( (this->vecGhostPts_) && (this->vecInteriorPts_) ) {
    std::vector<SS::IntVec>::const_iterator ig = (this->vecGhostPts_)->begin();    // ig is the ghost flat index
    std::vector<SS::IntVec>::const_iterator ii = (this->vecInteriorPts_)->begin(); // ii is the interior flat index
    if (this->isStaggered_) {
      if (side_==SS::PLUS_SIDE) {
        const double bcValue = (5 * t * sin((30 * PI )/(3 * t + 30)))/(( (t * t) + 1)*((5 / (exp(1125/(t + 10))*(2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125/(t + 10))*(2 * t + 5))));
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
          f(*ii) = ( bcValue - ci*f(*ig) ) / cg;
        }
      } else if (side_ == SS::MINUS_SIDE) {
        const double bcValue = (5 * t * sin((-30 * PI )/(3 * t + 30)))/(( (t * t) + 1)*((5 / (exp(1125/(t + 10))*(2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125/(t + 10))*(2 * t + 5))));
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
          f(*ii) = ( bcValue - ci*f(*ii) ) / cg;
        }
      }
    } else {
      if (side_==SS::PLUS_SIDE) {
        const double bcValue = (5 * t * sin((30 * PI )/(3 * t + 30)))/(( (t * t) + 1)*((5 / (exp(1125/(t + 10))*(2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125/(t + 10))*(2 * t + 5))));
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
        }
      } else if (side_ == SS::MINUS_SIDE) {
        const double bcValue = (5 * t * sin((-30 * PI )/(3 * t + 30)))/(( (t * t) + 1)*((5 / (exp(1125/(t + 10))*(2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125/(t + 10))*(2 * t + 5))));
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
        }
      }
    }
  }
}

// ###################################################################
//
//                          Implementation
//
// ###################################################################

template< typename FieldT >
void
VarDensMMSSolnVar<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  FieldT& f = this->value();
  const double ci = this->ci_;
  const double cg = this->cg_;
  const double t = (*t_)[0];  // this breaks GPU
  
  if ( (this->vecGhostPts_) && (this->vecInteriorPts_) ) {
    std::vector<SpatialOps::structured::IntVec>::const_iterator ig = (this->vecGhostPts_)->begin();    // ig is the ghost flat index
    std::vector<SpatialOps::structured::IntVec>::const_iterator ii = (this->vecInteriorPts_)->begin(); // ii is the interior flat index
    const double bcValue = -5/(exp(1125/(t + 10))*(2 * t + 5) * ((5/(exp(1125/(t + 10))*(2 * t + 5)) - 1)/rho0_ - 5/(rho1_ * exp(1125/(t + 10))*(2 * t + 5))));
    for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
      f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
    }
  }
}

// ###################################################################
//
//                          Implementation
//
// ###################################################################

template< typename FieldT >
void
VarDensMMSVelocity<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  namespace SS = SpatialOps::structured;
  
  FieldT& f = this->value();
  const double ci = this->ci_;
  const double cg = this->cg_;
  const double t = (*t_)[0];  // this breaks GPU
  
  if ( (this->vecGhostPts_) && (this->vecInteriorPts_) ) {
    std::vector<SS::IntVec>::const_iterator ig = (this->vecGhostPts_)->begin();    // ig is the ghost flat index
    std::vector<SS::IntVec>::const_iterator ii = (this->vecInteriorPts_)->begin(); // ii is the interior flat index
    if (this->isStaggered_) {
      if (side_== SS::PLUS_SIDE) {
        const double bcValue = ( ((-5 * t)/( t * t + 1)) * sin(10 * PI / (t + 10) ) );
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
          f(*ii) = ( bcValue - ci*f(*ig) ) / cg;
        }
      } else if (side_ == SS::MINUS_SIDE) {
        const double bcValue = ( ((-5 * t)/( t * t + 1)) * sin(-10 * PI / (t + 10) ) );
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
          f(*ii) = ( bcValue - ci*f(*ii) ) / cg;
        }
      }
    } else {
      if (side_== SS::PLUS_SIDE) {
        const double bcValue = ( ((-5 * t)/( t * t + 1)) * sin(10 * PI / (t + 10) ) );
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
        }
      } else if (side_ == SS::MINUS_SIDE) {
        const double bcValue = ( ((-5 * t)/( t * t + 1)) * sin(-10 * PI / (t + 10) ) );
        for( ; ig != (this->vecGhostPts_)->end(); ++ig, ++ii ){
          f(*ig) = ( bcValue - ci*f(*ii) ) / cg;
        }
      }
    }
  }
}

//------------------
#define INSTANTIATE_VARDEN_MMS_BCS(VOLT)          \
  template class VarDensMMSDensity<VOLT>;         \
  template class VarDensMMSMixtureFraction<VOLT>; \
  template class VarDensMMSSolnVar<VOLT>;         \
  template class VarDensMMSMomentum<VOLT>;        \
  template class VarDensMMSVelocity<VOLT>;

INSTANTIATE_VARDEN_MMS_BCS(SVolField)
INSTANTIATE_VARDEN_MMS_BCS(XVolField)
INSTANTIATE_VARDEN_MMS_BCS(YVolField)
INSTANTIATE_VARDEN_MMS_BCS(ZVolField)