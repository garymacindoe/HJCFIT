/***********************
    DCProgs computes missed-events likelihood as described in
    Hawkes, Jalali and Colquhoun (1990, 1992)

    Copyright (C) 2013  University College London

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
************************/

#include "DCProgsConfig.h"
#include <iostream>
#include <type_traits>
#include <gtest/gtest.h>
#include "../recursion_formula.h"
using namespace DCProgs;

struct FullRecursion {
  typedef t_real t_element;

  FullRecursion() {
     D.resize(5);
     D << 1, 2, 3, 4, 5;
     eigvals.resize(5);
     eigvals << 1./1, 1./2., 1./3., 1./4., 1./5.;
     initials.resize(5);
     initials << 1, 1, 1, 1, 1;
  }

  t_element operator()(t_uint _i, t_uint _j, t_uint _k) const {
    if(_j == 0 and _k == 0) return initials(_i);
    return recursion_formula(*this, _i, _j, _k);
  }
  t_element getD(t_uint _i) const { return D(_i); }
  t_element get_eigvals(t_uint _i) const { return eigvals(_i); }
  t_uint nbeigvals() const { return static_cast<t_uint>(eigvals.size()); }

  t_rvector D, eigvals, initials;
};


TEST(TestRecursion, m_equal_l_InitialZero) {
  FullRecursion fullrecursion;
  fullrecursion.initials << 0, 0, 0, 0, 0;

  for(t_uint i(0); i < 5; ++i) {
    for(t_uint m(0); m < 10; ++m) 
      EXPECT_DOUBLE_EQ(recursion_formula(fullrecursion, i, m, m), 0e0);
  }
}

t_uint factorial(t_uint _i) { return _i == 0 ? 1: _i * factorial(_i-1); }


TEST(TestRecursion, m_equal_l_InitialOnes) {
  FullRecursion fullrecursion;

  for(t_uint i(0); i < 5; ++i) {
    EXPECT_DOUBLE_EQ(recursion_formula(fullrecursion, i, 0, 0), fullrecursion.initials(i));
    for(t_uint m(1); m < 10; ++m)  {
      EXPECT_NEAR( recursion_formula(fullrecursion, i, m, m), 
                   std::pow(fullrecursion.D(i), m) / t_real(factorial(m)) * fullrecursion.initials(i),
                   1e-8 );
    }
  }
}

TEST(TestRecursion, m_equal_l_InitialRandom) {
  FullRecursion fullrecursion;
  for(t_uint trial(0); trial < 500; ++trial) {
    fullrecursion.initials = t_rvector::Random(5);
  
    for(t_uint i(0); i < 5; ++i) {
      EXPECT_DOUBLE_EQ(recursion_formula(fullrecursion, i, 0, 0), fullrecursion.initials(i));
      for(t_uint m(1); m < 10; ++m)  {
        EXPECT_NEAR( recursion_formula(fullrecursion, i, m, m), 
                     std::pow(fullrecursion.D(i), m) / t_real(factorial(m)) * fullrecursion.initials(i),
                     1e-8 );
      }
    }
  }
}
 
TEST(TestRecursion, lzero_InitialZero) {
  FullRecursion fullrecursion;
  fullrecursion.initials << 0, 0, 0, 0, 0;

  for(t_uint i(0); i < 5; ++i) {
    for(t_uint m(0); m < 5; ++m) 
      EXPECT_DOUBLE_EQ(recursion_formula(fullrecursion, i, m, 0), 0e0);
  }
}


class TestViaMatrix : public ::testing::TestWithParam<t_int> {
  public:
    struct Mocking {
      typedef t_real t_element;
      t_rvector D, eigvals;
      t_rmatrix valmat;
      t_uint current_m;
  
      t_element operator()(t_uint _i, t_uint _j, t_uint _k) const {
        EXPECT_EQ(_j, current_m) << "Unexpected m value in  recursion.";
        return valmat(_i, _k);
      }
      t_element getD(t_uint _i) const { return D(_i); }
      t_element get_eigvals(t_uint _i) const { return eigvals(_i); }
      t_uint nbeigvals() const { return static_cast<t_uint>(eigvals.size()); }
    };
  
    TestViaMatrix() {
       jay.D.resize(matsize);
       jay.eigvals.resize(matsize);
       for(t_uint i(0); i < matsize; ++i) {
         jay.D(i) = t_real(i+1); 
         jay.eigvals(i) = 1e-1 * t_real(i+1);
       }
    
       for(t_uint i(0); i < matsize; ++i) {
       
         t_rmatrix term0 = t_rmatrix::Zero(jay.nbeigvals(), mmax);
         t_rmatrix term1 = t_rmatrix::Zero(jay.nbeigvals(), mmax);
         for(t_uint j(0); j < jay.nbeigvals(); ++j) {
           if(i == j) continue;
           t_real const lambda_diff = 1e0 / (jay.eigvals(j) - jay.eigvals(i));
           t_real factor(lambda_diff);
           t_int s(1);
           for(t_uint r(0); r < mmax; ++r, s = -s) {
             term0(j, r) = factor * jay.D(i);
             term1(j, r) = s * factor * jay.D(j);
             factor *= (r + 1) * lambda_diff;
           }
         }
         terms0.push_back(term0);
         terms1.push_back(term1);
       }
    }
    

  protected:
    Mocking jay;
    //! \f$\frac{D_i}{(\lambda_i-\lambda_j)^{r+1}}, with i std::vector first index
    std::vector<t_rmatrix> terms0;
    //! \f$\frac{D_j}{(\lambda_i-\lambda_j)^{r+1}}, with i std::vector first index
    std::vector<t_rmatrix> terms1;
    //! \f$\frac{D_j}{(\lambda_i-\lambda_j)^{r+1}}, with i std::vector first index
    t_uint const static mmax = 8;
    t_uint const static matsize = 5;
};

TEST_P(TestViaMatrix, lzero) {

  // Create the matrix by which to multiply.
  jay.valmat = t_rmatrix::Zero(jay.nbeigvals(), mmax);
  switch(GetParam()) {
    case 0: jay.valmat(0, 0) = 1; break;
    case 1: jay.valmat(1, 1) = 1; break;
    case 2: jay.valmat = t_rmatrix::Ones(jay.nbeigvals(), mmax); break;
    default: jay.valmat = t_rmatrix::Random(jay.nbeigvals(), mmax);
  }


  // Now do checks
  for(t_uint i(0); i < matsize; ++i) {

    t_rmatrix const term0 = terms0[i].array() * jay.valmat.array();
    for(t_uint m(1); m < mmax; ++m) {
      jay.current_m = m - 1; // Checks we don't make inner calls
      t_real const term1= (terms1[i].leftCols(m)
                           * jay.valmat.row(i).head(m).transpose()).array().sum();
      
      t_real const check = term0.topLeftCorner(term0.rows(), m).sum() + term1; 
      EXPECT_NEAR(recursion_formula(jay, i, m, 0), check, std::abs(check) * 1e-8)
          << "i=" << i << " m=" << m;
                   
    }
  }
}

TEST_P(TestViaMatrix, general) {

  // Create the matrix by which to multiply.
  jay.valmat = t_rmatrix::Zero(jay.nbeigvals(), mmax);
  switch(GetParam()) {
    case 0: jay.valmat(0, 0) = 1; break;
    case 1: jay.valmat(1, 1) = 1; break;
    case 2: jay.valmat = t_rmatrix::Ones(jay.nbeigvals(), mmax); break;
    default: jay.valmat = t_rmatrix::Random(jay.nbeigvals(), mmax);
  }

  
  // Now do checks
  for(t_uint i(0); i < matsize; ++i) {
    t_rmatrix const &term = terms1[i];
    auto valrow = jay.valmat.row(i);
    for(t_uint m(2); m < mmax; ++m) {
      jay.current_m = m - 1; // Checks we don't make inner calls
      for(t_uint l(1); l < m; ++l) {


        //! dcl: \f$D_i C_{i(m-1)(l-1)}/l\f$
        t_real dcl = jay.D(i) * jay.valmat(i, l-1) / t_real(l);
        //! Now add other term to dcl
        for(t_uint j(0); j < matsize; ++j) {
          if(i == j) continue;
          t_real const factor = jay.D(j)  / term(j, l-1) / t_real(l);
          dcl -= (valrow.segment(l, m-l) * term.row(j).segment(l, m-l).transpose() * factor)(0, 0);
        }
          
        EXPECT_NEAR( recursion_formula(jay, i, m, l), dcl, std::abs(dcl) * 1e-8 )
          << "i=" << i << " m=" << m << " l=" << l;
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(SingleRecursion, TestViaMatrix, ::testing::Range(t_int(0), t_int(300)));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

