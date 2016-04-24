#ifndef Uintah_Component_Arches_DISCRETIZATIONTOOLS_h
#define Uintah_Component_Arches_DISCRETIZATIONTOOLS_h

#include <Core/Exceptions/InvalidValue.h>
#include <Core/Grid/Variables/BlockRange.h>
#ifdef UINTAH_ENABLE_KOKKOS
#include <Kokkos_Core.hpp>
#endif //UINTAH_ENABLE_KOKKOS

/** @class DiscretizationTools
    @author J. Thornock
    @date Dec, 2015

    @brief Provides some basic, commonly used/shared functionality for differencing
    grid variables.

**/

namespace Uintah{

  enum DIR {XDIR, YDIR, ZDIR};

#define STENCIL3_1D( dir ) \
  const int ip  = dir == 0 ? i+1 : i; \
  const int im  = dir == 0 ? i-1 : i; \
  const int jp  = dir == 1 ? j+1 : j; \
  const int jm  = dir == 1 ? j-1 : j; \
  const int kp  = dir == 2 ? k+1 : k; \
  const int km  = dir == 2 ? k-1 : k; \

#define STENCIL5_1D( dir ) \
  const int ip  = dir == 0 ? i+1 : i; \
  const int ipp = dir == 0 ? i+2 : i; \
  const int im  = dir == 0 ? i-1 : i; \
  const int imm = dir == 0 ? i-2 : i; \
  const int jp  = dir == 1 ? j+1 : j; \
  const int jpp = dir == 1 ? j+2 : j; \
  const int jm  = dir == 1 ? j-1 : j; \
  const int jmm = dir == 1 ? j-2 : j; \
  const int kp  = dir == 2 ? k+1 : k; \
  const int kpp = dir == 2 ? k+2 : k; \
  const int km  = dir == 2 ? k-1 : k; \
  const int kmm = dir == 2 ? k-2 : k

#define C_    i,   j,   k
#define CP_   ip,  jp,  kp
#define CPP_  ipp, jpp, kpp
#define CM_   im,  jm,  km
#define CMM_  imm, jmm, kmm

//Staggered rotation
#define CE_ i+ioff, j+joff, k+koff
#define CW_ i-ioff, j-joff, k-koff
#define CN_ i+koff, j+ioff, k+joff
#define CS_ i-koff, j-ioff, k-joff
#define CT_ i+joff, j+koff, k+ioff
#define CB_ i-joff, j-koff, k-ioff
#define CNE_ i+idt1,j+jdt1,k+kdt1
#define CNW_ i+inw,j+jnw,k+knw
#define CSE_ i-idt1,j-jdt1,k-kdt1
#define CSW_ i-isw,j-jsw,k-ksw
#define CTE_ i+idt2,j+jdt2,k+kdt2
#define CTW_ i+itw,j+jtw,k+ktw
#define CBE_ i-idt2,j-jdt2,k-kdt2
#define CBW_ i-ibw,j-jbw,k-kbw
#define C2E_ i+i2off, j+j2off, k+k2off
#define C2W_ i-i2off, j-j2off, k-k2off

#define PRINT_CURR_REFERENCE(i,j,k,string) \
  std::cout << "Location " << string << " = (" << i << "," << j << "," << k << ")" << std::endl;

#define STAGGERED_INDEX(dir) \
  const int ioff = dir == 0 ? 1 : 0;

  /**
      @struct VariableHelper
      @details Useful for reconciling the const and non-const types for a specific
      template parameter. Also useful for deducing face types.
  **/
  template <typename T>
  struct VariableHelper{
  };

  //Helper specialization:
  template <>
  struct VariableHelper<CCVariable<double> >{
    typedef constCCVariable<double> ConstType;
    typedef CCVariable<double> Type;
    typedef SFCXVariable<double> XFaceType;
    typedef SFCYVariable<double> YFaceType;
    typedef SFCZVariable<double> ZFaceType;
    typedef constSFCXVariable<double> ConstXFaceType;
    typedef constSFCYVariable<double> ConstYFaceType;
    typedef constSFCZVariable<double> ConstZFaceType;
    int dir;
    VariableHelper():dir(999){}
  };

  template <>
  struct VariableHelper<SFCXVariable<double> >{
    typedef constSFCXVariable<double> ConstType;
    typedef SFCXVariable<double> Type;
    typedef CCVariable<double> XFaceType;
    typedef SFCYVariable<double> YFaceType;
    typedef SFCZVariable<double> ZFaceType;
    typedef constCCVariable<double> ConstXFaceType;
    typedef constSFCYVariable<double> ConstYFaceType;
    typedef constSFCZVariable<double> ConstZFaceType;
    DIR dir;
    const int ioff;
    const int joff;
    const int koff;
    const int idt1;
    const int idt2;
    const int jdt1;
    const int jdt2;
    const int kdt1;
    const int kdt2;
    VariableHelper():dir(XDIR), ioff(1), joff(0), koff(0),
    idt1(koff), idt2(joff), jdt1(ioff), jdt2(koff), kdt1(joff), kdt2(ioff){}
  };

  template <>
  struct VariableHelper<SFCYVariable<double> >{
    typedef constSFCYVariable<double> ConstType;
    typedef SFCYVariable<double> Type;
    typedef SFCXVariable<double> XFaceType;
    typedef CCVariable<double> YFaceType;
    typedef SFCZVariable<double> ZFaceType;
    typedef constSFCXVariable<double> ConstXFaceType;
    typedef constCCVariable<double> ConstYFaceType;
    typedef constSFCZVariable<double> ConstZFaceType;
    DIR dir;
    const int ioff;
    const int joff;
    const int koff;
    const int idt1;
    const int idt2;
    const int jdt1;
    const int jdt2;
    const int kdt1;
    const int kdt2;
    VariableHelper():dir(XDIR), ioff(1), joff(0), koff(0),
    idt1(koff), idt2(joff), jdt1(ioff), jdt2(koff), kdt1(joff), kdt2(ioff){}
  };

  template <>
  struct VariableHelper<SFCZVariable<double> >{
    typedef constSFCZVariable<double> ConstType;
    typedef SFCZVariable<double> Type;
    typedef SFCXVariable<double> XFaceType;
    typedef SFCYVariable<double> YFaceType;
    typedef CCVariable<double> ZFaceType;
    typedef constSFCXVariable<double> ConstXFaceType;
    typedef constSFCYVariable<double> ConstYFaceType;
    typedef constCCVariable<double> ConstZFaceType;
    DIR dir;
    const int ioff;
    const int joff;
    const int koff;
    const int idt1;
    const int idt2;
    const int jdt1;
    const int jdt2;
    const int kdt1;
    const int kdt2;
    VariableHelper():dir(XDIR), ioff(1), joff(0), koff(0),
    idt1(koff), idt2(joff), jdt1(ioff), jdt2(koff), kdt1(joff), kdt2(ioff){}
  };

  //------------------------------------------------------------------------------------------------
  // These stucts below need a good home:

  /**
      @struct ComputeDiffusion
      @details Compute the diffusion term for a scalar.
      This doesn't have a good home at the moment so it resides here.
      @TODO Currently only works for CCVariables. Generalize it?
      @TODO Fix it to work with boundaries.
  **/
  template <typename T, typename FT>
  struct ComputeDiffusion{

    //for now assuming that this will only be used for CC variables:
    typedef typename VariableHelper<T>::ConstType ConstPT;
    typedef typename VariableHelper<T>::Type PT;

    ConstPT& phi;
    constCCVariable<double>& gamma; //!!!!!!!
    FT& af;
    PT& rhs;
    IntVector dir;
    double A;
    double dx;

    ComputeDiffusion( ConstPT& phi, constCCVariable<double>& gamma, PT& rhs, FT& af,
      IntVector dir, double A, double dx ) :
      phi(phi), gamma(gamma), rhs(rhs), af(af), dir(dir), A(A), dx(dx) { }

    void operator()(int i, int j, int k) const {

      IntVector c(i,j,k);
      IntVector cm(i-dir[0],j-dir[1],k-dir[2]);
      IntVector cp(i+dir[0],j+dir[1],k+dir[2]);

      double face_gamma_e = (gamma[c] + gamma[cp])/ 2.0 * af[cp];
      double face_gamma_w = (gamma[c] + gamma[cm])/ 2.0 * af[c];

      double grad_e = (phi[cp]-phi[c])/dx;
      double grad_w = (phi[c]-phi[cm])/dx;

      rhs[c] += A * ( face_gamma_e * grad_e - face_gamma_w * grad_w );

    }
  };

  //discretization
  class DiscretizationTools{
  public:

  private:

  };
}
#endif
