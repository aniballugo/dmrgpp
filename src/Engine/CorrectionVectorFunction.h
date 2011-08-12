// BEGIN LICENSE BLOCK
/*
Copyright (c) 2009-2011, UT-Battelle, LLC
All rights reserved

[DMRG++, Version 2.0.0]
[by G.A., Oak Ridge National Laboratory]

UT Battelle Open Source Software License 11242008

OPEN SOURCE LICENSE

Subject to the conditions of this License, each
contributor to this software hereby grants, free of
charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), a
perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable copyright license to use, copy,
modify, merge, publish, distribute, and/or sublicense
copies of the Software.

1. Redistributions of Software must retain the above
copyright and license notices, this list of conditions,
and the following disclaimer.  Changes or modifications
to, or derivative works of, the Software should be noted
with comments and the contributor and organization's
name.

2. Neither the names of UT-Battelle, LLC or the
Department of Energy nor the names of the Software
contributors may be used to endorse or promote products
derived from this software without specific prior written
permission of UT-Battelle.

3. The software and the end-user documentation included
with the redistribution, with or without modification,
must include the following acknowledgment:

"This product includes software produced by UT-Battelle,
LLC under Contract No. DE-AC05-00OR22725  with the
Department of Energy."
 
*********************************************************
DISCLAIMER

THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER, CONTRIBUTORS, UNITED STATES GOVERNMENT,
OR THE UNITED STATES DEPARTMENT OF ENERGY BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

NEITHER THE UNITED STATES GOVERNMENT, NOR THE UNITED
STATES DEPARTMENT OF ENERGY, NOR THE COPYRIGHT OWNER, NOR
ANY OF THEIR EMPLOYEES, REPRESENTS THAT THE USE OF ANY
INFORMATION, DATA, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.

*********************************************************


*/
// END LICENSE BLOCK
/** \ingroup DMRG */
/*@{*/

/*! \file CorrectionVectorFunction.h
 *
 *  This is an implementation of PRB 60, 225, Eq. (24)
 * 
 */
#ifndef CORRECTION_V_FUNCTION_H
#define CORRECTION_V_FUNCTION_H
#include "ConjugateGradient.h"

namespace Dmrg {
	template<typename RealType,typename MatrixType,typename InfoType>
	class	CorrectionVectorFunction {

		typedef typename MatrixType::value_type FieldType;
		typedef std::vector<FieldType> VectorType;

		class InternalMatrix {
		public:
			typedef FieldType value_type ;
			InternalMatrix(const MatrixType& m,const InfoType& info)
			: m_(m),info_(info) {}

			size_t rank() const { return m_.rank(); }

			void matrixVectorProduct(VectorType& x,const VectorType& y) const
			{
				RealType eta = info_.eta;
				RealType omega = info_.omega;
				VectorType xTmp(x.size(),0);
				m_.matrixVectorProduct(xTmp,y); // xTmp = Hy
				VectorType x2(x.size(),0);
				m_.matrixVectorProduct(x2,x); // x2 = H^2 y
				x = x2 -2 *omega * xTmp + (omega*omega + eta*eta)*y;
				x /= (-eta);
			}
		private:
			const MatrixType& m_;
			const InfoType& info_;
		};
		typedef ConjugateGradient<RealType,InternalMatrix> ConjugateGradientType;
	public:
		CorrectionVectorFunction(const MatrixType& m,const InfoType& info)
		: im_(m,info),cg_()
		{
		}

		void getXi(VectorType& result,const VectorType& sv) const
		{
			std::vector<VectorType> x;
			VectorType x0(result.size(),0);
			x.push_back(x0); // initial ansatz
			cg_(x,im_,sv);
			size_t k = x.size();
			result = x[k-1];
		}

	private:
		InternalMatrix im_;
		ConjugateGradientType cg_;
	}; // class CorrectionVectorFunction
} // namespace Dmrg

/*@}*/
#endif // CORRECTION_V_FUNCTION_H

