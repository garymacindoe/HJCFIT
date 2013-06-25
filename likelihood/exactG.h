#ifndef DCPROGS_LIKELIHOOD_EXACTG_H
#define DCPROGS_LIKELIHOOD_EXACTG_H

#include <DCProgsConfig.h>

#include <tuple>
#include <map>
#include <memory>


#include "state_matrix.h"
#include "recursion_formula.h"

namespace DCProgs {

  //! \brief Implementation of recursion for exact missed-event G function
  //! \details Implements the exact-missed event probability calculations, as detailed in Hawkes,
  //! Jalali, and Colquhoun (1990). Specifically, this is equation 3.2.
  class ExactG {
    //! Just trying to figure out a complex return type...
    typedef decltype( t_rmatrix::Zero(1, 1) ) t_Zero; 

    public:
      //! Initializes exact G functor.
      ExactG(StateMatrix const &_matrix, t_real _tau) { set(_matrix, _tau); }


      //! Sets the values for which to compute exact g.
      void set(StateMatrix const &_matrix, t_real _tau);

      //! Open to close transitions 
      t_rmatrix af(t_real t) const;
      //! Close to open transitions
      t_rmatrix fa(t_real t) const;

    protected:
      //! \brief Implementation of recursion for exact missed-event G function
      //! \details This is an interface to the function recursion_formula.  In practice, this object
      //!          needs not be called directly. Rather the public interface (which is about
      //!          computing the likelihood for an event of duration t) is in the containing class
      //!          ExactG.
      class RecursionInterface;

#     ifndef HAS_CXX11_UNIQUE_PTR
        //! Type of the pointers holding recursion interfaces.
        typedef std::auto_ptr<RecursionInterface> t_RecursionPtr;
#     else
        //! Type of the pointers holding recursion interfaces.
        typedef std::unique_ptr<RecursionInterface> t_RecursionPtr;
#     endif
      //! Pointer to AF recursion interface
      t_RecursionPtr recursion_af_;
      //! Pointer to FA recursion interface
      t_RecursionPtr recursion_fa_;
      //! Max length of missed events.
      t_real tau_;
      //! \f$Q_{AF}e^{-Q_{FF}\tau} \f$
      t_rmatrix af_factor_;
      //! \f$Q_{FA}e^{-Q_{AA}\tau} \f$
      t_rmatrix fa_factor_;
  };


  class ExactG::RecursionInterface {
  
    public:
      //! Element on which to perform recursion.
      typedef t_rmatrix t_element;
      //! Constructor. 
      //! \param[in] _matrix: The transition state matrix for which to compute
      //!                     \f$^eG_{AF}(t\rightarrow\infty)\f$
      //! \param[in] _doAF: Whether to do AF (true) or FA.
      RecursionInterface(StateMatrix const & _matrix, t_real _tau, bool _doAF=true);
  
      //! Recursion element i, m, l.
      t_element operator()(t_int _i, t_int _m, t_int _l);
  
      //! Returns D values.
      t_element const & getD(t_int _i) const {
        assert(_i >= 0 and _i <= nbeigvals());
        return dvalues_[_i]; 
      }
      //! Returns ith eigenvalue.
      t_real get_eigvals(t_int _i) const {
        assert(_i >= 0 and _i <= nbeigvals());
        return eigenvalues_(_i);
      }
      //! Returns the number of eigenvalues
      t_int nbeigvals() const { return eigenvalues_.size(); }
  
    protected:
      //! Key of the map where coefficient matrices are stored.
      typedef std::tuple<t_int, t_int, t_int> t_key;
      //! Map where coefficients are stored.
      std::map<t_key, t_element> coeff_map_;
      //! D matrices (See equation 3.16)
      std::vector<t_element> dvalues_;
      //! Eigenvalues of the transition rate matrix.
      t_rvector eigenvalues_;
      //! Number of open states.
      t_int nopen;
  };
}

#endif 
