#include <DCProgsConfig.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

#include <unsupported/Eigen/MatrixFunctions>

#include "errors.h"
#include "exact_survivor.h"

namespace DCProgs {

  // anonymous namespace -- functions inside are not expected in the shared library.
  namespace {
    // Computes lowest two eigenvalues,
    // Checks they are real
    // Checks they are positive
    // Checks they are not equal,
    // Checks the lowest is below tolerance,
    // Returns something above lowest and below second lowest.
    t_real lowest_eigenvalue(t_cvector const &_eigs, t_real _tolerance=1e-8) {

      // NOTE: neg_eig will be -_eigs.real(). Watch sign. 
      std::vector<t_real> neg_eigs(_eigs.size());
      std::vector<t_real> :: iterator i_first = neg_eigs.begin();
      t_cvector::Scalar const * ptr_eig = &_eigs[0];
      t_cvector::Scalar const * const ptr_eig_end = ptr_eig + _eigs.size();
      for(; ptr_eig != ptr_eig_end; ++ptr_eig, ++i_first) {
        if(std::abs(ptr_eig->imag()) > _tolerance) 
          throw errors::ComplexEigenvalues("Transition matrix has complex eigenvalues.");
        *i_first = -ptr_eig->real();
      }

      std::partial_sort(neg_eigs.begin(), neg_eigs.begin() + 2, neg_eigs.end());
      if( neg_eigs.front() < -_tolerance )
        throw errors::Mass("Expected eigenvalues of transition matrix to be negative.");
      if( neg_eigs[0] > _tolerance)
        throw errors::Mass("Kernel of transition matrix is of dimension 0.");
      if( neg_eigs[1] - neg_eigs[0] < _tolerance)
        throw errors::Mass("Kernel of transition matrix is larger than 1.");
      return (neg_eigs[1] + neg_eigs[0]) * 0.5;
    }
  }

  void ExactSurvivor :: set(QMatrix const &_matrix, t_real _tau) {
    if(_tau <= 0e0) throw errors::Domain("The resolution time tau cannot be zero or negative.");

    // Two step process. Otherwise, reset would catch any exception thrown. 
    RecursionInterface afinterface(_matrix, _tau, true);
    RecursionInterface fainterface(_matrix, _tau, false);
    recursion_af_.reset(new RecursionInterface(std::move(afinterface)));
    if(not recursion_af_.get()) throw errors::Runtime("Could not initialize unique_ptr");
    recursion_fa_.reset(new RecursionInterface(std::move(fainterface)));
    if(not recursion_fa_.get()) throw errors::Runtime("Could not initialize unique_ptr");

    tau_ = _tau;
  }


  ExactSurvivor :: RecursionInterface::RecursionInterface( QMatrix const & _matrix,
                                                           t_real _tau, bool _doAF ) {
                   
    // Sets matrix depending on whether this is AF or FA stuff.
    QMatrix const transitions = _doAF ? _matrix: _matrix.transpose();

    // Solves eigenvalue problem
    Eigen::EigenSolver<t_rmatrix> eigsolver(transitions.matrix);
    if(eigsolver.info() != Eigen::Success) 
        throw errors::Mass("Could not solve eigenvalue problem.");

    // Initializes eigenvalues
    t_cvector const & eigenvalues = eigsolver.eigenvalues();
    t_real const mineig = lowest_eigenvalue(eigenvalues);
    eigenvalues_.resize(eigsolver.eigenvalues().size()-1);

    // Initializes recursion formula for m == l == 0
    t_rmatrix const eigenvectors = eigsolver.eigenvectors().real();
    t_rmatrix const eigenvectors_inv = eigenvectors.inverse();
    for(t_int i(0), j(0); i < eigenvalues.size(); ++i) {
      if(-eigenvalues(i).real() < mineig) continue;
      auto left = eigenvectors.col(i).head(transitions.nopen);
      auto right = eigenvectors_inv.row(i).head(transitions.nopen);
      coeff_map_[ std::make_tuple(j, 0, 0) ] = left * right;
      eigenvalues_(j) = -eigenvalues(i).real();
      ++j;
    }

    // Computes all Di values
    t_rmatrix const exponential_factor = (_tau * transitions.ff()).exp() * transitions.fa();
    for(t_int i(0); i < eigenvalues.size(); ++i) {
      if(-eigenvalues(i).real() < mineig) continue;
      auto left = eigenvectors.col(i).head(transitions.nopen);
      auto right = eigenvectors_inv.row(i).tail(transitions.nclose());
      dvalues_.push_back((left * right) * exponential_factor);
    }
    // set number of open states
    nopen = transitions.nopen;
  }

  // Recursion element i, m, l.
  ExactSurvivor::RecursionInterface::t_element
    ExactSurvivor::RecursionInterface::operator()(t_int _i, t_int _m, t_int _l) {

      assert(_i >= 0 and _m >= 0 and _l >= 0);
      assert(_i < nbeigvals());

      // Checks for existence in cache.
      t_key const key(_i, _m, _l);
      std::map<t_key, t_element>::const_iterator const i_found = coeff_map_.find(key);
      if(i_found != coeff_map_.end()) return i_found->second;

      // Otherwise compute it from recursion
      t_int const NOPEN = this->nopen;
      return recursion_formula(*this, _i, _m, _l);
    }

  namespace {
    
    //! \brief Computes \f$B_{im}(t) = \sum_{r=0}^m C_{imr}t^r\f$
    //! \details See Theorem below equation 3.12
    template<class T> 
      typename T::t_element B_im_of_t(T &_C, t_int _i, t_int _m, t_real _t) {
        t_rmatrix result = _C(_i, _m, 0);
        t_real t(_t);
        for(t_int r(1); r <= _m; ++r, t *= _t) result += _C(_i, _m, r) * t;
        return result;
      }
    //! \brief Computes \f$M_m(t) = \sum_{i=1}^k B_{im}(t) e^{-\lambda_i t}\f$
    //! \details See Theorem below equation 3.12
    template<class T> 
      typename T::t_element M_m_of_t(T &_C, t_int _m, t_real _t) {

        // Eigenvalues == 0 are special (eq. solutions) and should be avoided.
        t_int i(0);
        for(; i < _C.nbeigvals(); ++i) 
          if( std::abs(_C.get_eigvals(i)) > 1e-10 ) break;

        t_rmatrix result = B_im_of_t(_C, i, _m, _t) * std::exp(-_C.get_eigvals(i)*_t);
        for(++i; i < _C.nbeigvals(); ++i)
          if( std::abs(_C.get_eigvals(i)) > 1e-10 ) 
            result += B_im_of_t(_C, i, _m, _t) * std::exp(-_C.get_eigvals(i)*_t);
        return result;
      }
    
    //! \brief Computes \f$R(t) = \sum_{m=0}^{t-m\tau>0} M_{m}(t-m\tau)\f$
    //! \details Equation 3.12 of HJC (1990)
    template<class T> 
      typename T::t_element R_of_t(T &_C, t_real _t, t_real _tau) {

        t_real current_t(_t);
        t_rmatrix result = M_m_of_t(_C, 0, _t);
        t_int m=1;
        
        for(current_t -= _tau; current_t > 0; current_t -= _tau, ++m)  {
          if(m % 2 == 0) result += M_m_of_t(_C, m, current_t);
          else           result -= M_m_of_t(_C, m, current_t);
        }
        return result;
      }

  }

  t_rmatrix ExactSurvivor :: af(t_real _t) const {
    if(_t < 0e0) return recursion_af_->zero();
    return R_of_t(*recursion_af_, _t, tau_);
  }
  t_rmatrix ExactSurvivor :: fa(t_real _t) const {
    if(_t < 0e0) return recursion_af_->zero();
    return R_of_t(*recursion_fa_, _t, tau_);
  }

  t_rmatrix ExactSurvivor :: recursion_af(t_int _i, t_int _m, t_int _l) const {
    if(_i < 0 or _m < 0 or _l <0) throw errors::Index("Indices should be positive.");
    if(_i >= recursion_af_->nbeigvals())  
      throw errors::Index("i index should be smaller than the number of eigenvalues.");
    if(_l > _m) throw errors::Index("l index should be smaller than m index.");
    return  recursion_af_->operator()(_i, _m, _l);
  }
  t_rmatrix ExactSurvivor :: recursion_fa(t_int _i, t_int _m, t_int _l) const {
    if(_i < 0 or _m < 0 or _l <0) throw errors::Index("Indices should be positive.");
    if(_i >= recursion_fa_->nbeigvals())  
      throw errors::Index("i index should be smaller than the number of eigenvalues.");
    if(_l > _m) throw errors::Index("l index should be smaller than m index.");
    return  recursion_fa_->operator()(_i, _m, _l);
  }
  t_rmatrix ExactSurvivor :: D_af(t_int _i) const {
    if(_i < 0) throw errors::Index("Indices should be positive.");
    if(_i >= recursion_af_->nbeigvals())  
      throw errors::Index("i index should be smaller than the number of eigenvalues.");
    return  recursion_af_->getD(_i);
  }
  t_rmatrix ExactSurvivor :: D_fa(t_int _i) const {
    if(_i < 0) throw errors::Index("Indices should be positive.");
    if(_i >= recursion_fa_->nbeigvals())  
      throw errors::Index("i index should be smaller than the number of eigenvalues.");
    return  recursion_fa_->getD(_i);
  }
  t_rvector ExactSurvivor::eigenvalues_af() const { return recursion_af_->eigenvalues(); }
  t_rvector ExactSurvivor::eigenvalues_fa() const { return recursion_af_->eigenvalues(); }
}
