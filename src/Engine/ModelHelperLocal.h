// BEGIN LICENSE BLOCK
/*
Copyright (c) 2009, UT-Battelle, LLC
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
#ifndef MODELHELPER_LOC_HEADER_H
#define MODELHELPER_LOC_HEADER_H

//#include "RightLeftLocal.h"
#include "PackIndices.h" // in PsimagLite
#include "Link.h"

/** \ingroup DMRG */
/*@{*/

/*! \file ModelHelperLocal.h
 *
 *  A class to contain state information about the Hamiltonian to help with the calculation of x+=Hy
 *
 */

namespace Dmrg { 	
	template<typename LeftRightSuperType_,
		typename ReflectionSymmetryType_,
		typename ConcurrencyType_>
	class ModelHelperLocal {

		typedef PsimagLite::PackIndices PackIndicesType;

	public:	
		typedef LeftRightSuperType_ LeftRightSuperType;
		typedef typename LeftRightSuperType::OperatorsType OperatorsType;
		typedef typename OperatorsType::SparseMatrixType SparseMatrixType;
		typedef typename SparseMatrixType::value_type SparseElementType;
		typedef typename OperatorsType::OperatorType OperatorType;
		typedef ReflectionSymmetryType_ ReflectionSymmetryType;
		typedef ConcurrencyType_ ConcurrencyType;
		typedef typename OperatorsType::BasisType BasisType;
		typedef typename BasisType::BlockType BlockType;
		typedef typename BasisType::RealType RealType;
		typedef typename LeftRightSuperType::BasisWithOperatorsType
				BasisWithOperatorsType;
		//typedef RightLeftLocal<BasisType,BasisWithOperatorsType,SparseMatrixType> RightLeftLocalType;
		typedef Link<SparseElementType,RealType> LinkType;
		
		static const size_t System=0,Environ=1;
		
		ModelHelperLocal(
				size_t m,
				const LeftRightSuperType& lrs,
				size_t orbitals,
				bool useReflection=false)
		:
			m_(m),
			lrs_(lrs),
			buffer_(lrs_.left().size()),
			basis2tc_(lrs_.left().numberOfOperators()),
			basis3tc_(lrs_.right().numberOfOperators()),
			alpha_(lrs_.super().size()),
			beta_(lrs_.super().size()),
			reflection_(useReflection),
			numberOfOperators_(lrs_.left().numberOfOperatorsPerSite())
			//,rightLeftLocal_(m,basis1,basis2,basis3,orbitals,useReflection)
		{
			createBuffer();
			createTcOperators(basis2tc_,lrs_.left());
			createTcOperators(basis3tc_,lrs_.right());
			createAlphaAndBeta();
		}

		size_t m() const { return m_; }

		static bool isSu2() { return false; }

//		const BasisType& basis1() const { return basis1_; }
//
//		const BasisWithOperatorsType& basis2() const  { return basis2_; }
//
//		const BasisWithOperatorsType& basis3() const  { return basis3_; }

		const SparseMatrixType& getReducedOperator(
				char modifier,
				size_t i,
				size_t sigma,
				size_t type) const
		{
			size_t ii = i*numberOfOperators_+sigma;
			if (modifier=='N') {
				if (type==System) return lrs_.left().getOperatorByIndex(ii).data;
				return lrs_.right().getOperatorByIndex(ii).data;
			}
			return getTcOperator(ii,type);
		}

		int size() const
		{
			int tmp = lrs_.super().partition(m_+1)-lrs_.super().partition(m_);
			return reflection_.size(tmp);
		}

		int quantumNumber() const
		{
			int state = lrs_.super().partition(m_);
			return lrs_.super().qn(state);
		}

		//! Does matrixBlock= (AB), A belongs to pSprime and B
		// belongs to pEprime or viceversa (inter)
		void fastOpProdInter(SparseMatrixType const &A,
				SparseMatrixType const &B,
				SparseMatrixType &matrixBlock,
				const LinkType& link,
				bool flipped = false) const
		{
			//int const SystemEnviron=1,EnvironSystem=2;
			RealType fermionSign =
					(link.fermionOrBoson==ProgramGlobals::FERMION) ? -1 : 1;
			
			//! work only on partition m
			if (link.type==ProgramGlobals::ENVIRON_SYSTEM)  {
				LinkType link2 = link;
				link2.value *= fermionSign;
				link2.type = ProgramGlobals::SYSTEM_ENVIRON; 
				fastOpProdInter(B,A,matrixBlock,link2,true);
				return;
			}		

			int m = m_;
			int offset = lrs_.super().partition(m);
			int total = lrs_.super().partition(m+1) - offset;
			int counter=0;
			matrixBlock.resize(total);

			int i;
			for (i=0;i<total;i++) {
				// row i of the ordered product basis
				matrixBlock.setRow(i,counter);
				int alpha=alpha_[i];
				int beta=beta_[i];		
							
				for (int k=A.getRowPtr(alpha);k<A.getRowPtr(alpha+1);k++) {
					int alphaPrime = A.getCol(k);
					for (int kk=B.getRowPtr(beta);kk<B.getRowPtr(beta+1);kk++) {
						int betaPrime= B.getCol(kk);
						int j = buffer_[alphaPrime][betaPrime];
						if (j<0) continue;
						/* fermion signs note:
						   here the environ is applied first and has to "cross"
						   the system, hence the sign factor pSprime.fermionicSign(alpha,tmp)
						  */
						SparseElementType tmp = A.getValue(k) * B.getValue(kk)*link.value;
						if (link.fermionOrBoson == ProgramGlobals::FERMION) 
							tmp *= lrs_.left().fermionicSign(alpha,int(fermionSign));
						//if (tmp==static_cast<MatrixElementType>(0.0)) continue;
						matrixBlock.pushCol(j);
						matrixBlock.pushValue(tmp);
						counter++;
					}
				}				
			} 
			matrixBlock.setRow(i,counter);	
		}
		
		//! Does x+= (AB)y, where A belongs to pSprime and B  belongs to pEprime or viceversa (inter)
		//! Has been changed to accomodate for reflection symmetry
		void fastOpProdInter(	std::vector<SparseElementType>  &x,
					std::vector<SparseElementType>  const &y,
					SparseMatrixType const &A,
					SparseMatrixType const &B,
					const LinkType& link,
				    	bool flipped = false) const
		{
			//int const SystemEnviron=1,EnvironSystem=2;
			RealType fermionSign =  (link.fermionOrBoson==ProgramGlobals::FERMION) ? -1 : 1;

			if (link.type==ProgramGlobals::ENVIRON_SYSTEM)  {
				LinkType link2 = link;
				link2.value *= fermionSign;
				link2.type = ProgramGlobals::SYSTEM_ENVIRON; 
				fastOpProdInter(x,y,B,A,link2,true);
				return;
			}
			
			//! work only on partition m
			int m = m_;
			int offset = lrs_.super().partition(m);
			int total = lrs_.super().partition(m+1) - offset;

			for (int i=0;i<total;i++) {
				if (reflection_.outsideReflectionBounds(i)) continue;
				// row i of the ordered product basis
				//utils::getCoordinates(alpha,beta,modelHelper.basis1().permutation(i+offset),ns);
				int alpha=alpha_[i];
				int beta=beta_[i];		
				for (int k=A.getRowPtr(alpha);k<A.getRowPtr(alpha+1);k++) {
					int alphaPrime = A.getCol(k);
					for (int kk=B.getRowPtr(beta);kk<B.getRowPtr(beta+1);kk++) {
						int betaPrime= B.getCol(kk);
						int j = buffer_[alphaPrime][betaPrime];
						if (j<0) continue;
						
						/* fermion signs note:
						   here the environ is applied first and has to "cross"
						   the system, hence the sign factor pSprime.fermionicSign(alpha,tmp)
						 */
						SparseElementType tmp = A.getValue(k) * B.getValue(kk)*link.value;
						
						if (link.fermionOrBoson == ProgramGlobals::FERMION)
							tmp *= lrs_.left().fermionicSign(alpha,int(fermionSign));
						//if (tmp==static_cast<MatrixElementType>(0.0)) continue;
						reflection_.elementMultiplication(tmp , x,y,i,j);
						//matrixBlock.pushCol(j-offset);
						//matrixBlock.pushValue(tmp);
					}
				}
			}
		}

		//! Let H_{alpha,beta; alpha',beta'} = basis2.hamiltonian_{alpha,alpha'} \delta_{beta,beta'}
		//! Let H_m be  the m-th block (in the ordering of basis1) of H
		//! Then, this function does x += H_m * y
		//! This is a performance critical function
		//! Has been changed to accomodate for reflection symmetry
		void hamiltonianLeftProduct(std::vector<SparseElementType> &x,std::vector<SparseElementType> const &y) const 
		{ 
			int m = m_;
			int offset = lrs_.super().partition(m);
			int i,k,alphaPrime;
			int bs = lrs_.super().partition(m+1)-offset;
			SparseMatrixType hamiltonian = lrs_.left().hamiltonian();
			size_t ns = lrs_.left().size();

			PackIndicesType pack(ns);
			for (i=0;i<bs;i++) {
				if (reflection_.outsideReflectionBounds(i)) continue;
				size_t r,beta;
				pack.unpack(r,beta,lrs_.super().permutation(i+offset));

				// row i of the ordered product basis
				for (k=hamiltonian.getRowPtr(r);k<hamiltonian.getRowPtr(r+1);k++) {
					alphaPrime = hamiltonian.getCol(k);
					//j = basis1_.permutationInverse(alphaPrime + betaPrimeNs)-offset;
					//if (j<0 || j>=bs) continue;
					int j = buffer_[alphaPrime][beta];
					if (j<0) continue;
					reflection_.elementMultiplication(hamiltonian.getValue(k), x,y,i,j);
				}
			}
		}

		//! Let  H_{alpha,beta; alpha',beta'} = basis2.hamiltonian_{beta,beta'} \delta_{alpha,alpha'}
		//! Let H_m be  the m-th block (in the ordering of basis1) of H
		//! Then, this function does x += H_m * y
		//! This is a performance critical function
		void hamiltonianRightProduct(std::vector<SparseElementType> &x,std::vector<SparseElementType> const &y) const 
		{ 
			int m = m_;
			int offset = lrs_.super().partition(m);
			int i,k;
			int bs = lrs_.super().partition(m+1)-offset;
			SparseMatrixType hamiltonian = lrs_.right().hamiltonian();
			size_t ns = lrs_.left().size();

			PackIndicesType pack(ns);
			for (i=0;i<bs;i++) {
				if (reflection_.outsideReflectionBounds(i)) continue;
				size_t alpha,r;
				pack.unpack(alpha,r,lrs_.super().permutation(i+offset));

				// row i of the ordered product basis
				for (k=hamiltonian.getRowPtr(r);k<hamiltonian.getRowPtr(r+1);k++) {
					
					//betaPrimeNs  = hamiltonian.getCol(k) *ns;
					//j = basis1_.permutationInverse(alpha + betaPrimeNs)-offset;
					//if (j<0 || j>=bs) continue;
					int j = buffer_[alpha][hamiltonian.getCol(k)];
					if (j<0) continue;
					reflection_.elementMultiplication(hamiltonian.getValue(k) , x,y,i,j);
				}
			}
		}

		//! if option==true let H_{alpha,beta; alpha',beta'} = basis2.hamiltonian_{alpha,alpha'} \delta_{beta,beta'}
		//! if option==false let  H_{alpha,beta; alpha',beta'} = basis2.hamiltonian_{beta,beta'} \delta_{alpha,alpha'}
		//! returns the m-th block (in the ordering of basis1) of H
		//! Note: USed only for debugging
		void calcHamiltonianPart(SparseMatrixType &matrixBlock,bool option) const 
		{ 
			int m  = m_;
			size_t offset = lrs_.super().partition(m);
			int k,alphaPrime=0,betaPrime=0;
			int bs = lrs_.super().partition(m+1)-offset;
			size_t ns=lrs_.left().size();
			SparseMatrixType hamiltonian;
			if (option) {
				hamiltonian = lrs_.left().hamiltonian();
				//ns = basis2_.size();
			} else {
				hamiltonian = lrs_.right().hamiltonian();
				//ns = 
			}
			std::cerr<<__FILE__<<":"<<__LINE__<<":\n";
			PsimagLite::Matrix<SparseElementType> fullm;
			crsMatrixToFullMatrix(fullm,hamiltonian);
			//printNonZero(fullm,std::cerr);
			matrixBlock.resize(bs);
			
			int counter=0;
			PackIndicesType pack(ns);
			for (size_t i=offset;i<lrs_.super().partition(m+1);i++) {
				matrixBlock.setRow(i-offset,counter);
				size_t alpha,beta;
				pack.unpack(alpha,beta,lrs_.super().permutation(i));
				size_t r=beta;
				if (option) {
					betaPrime=beta;
					r = alpha;
				} else {
					alphaPrime=alpha;
					r=beta;
				}	
				if (r>=hamiltonian.rank()) 
					throw std::runtime_error("DrmgModelHelper::calcHamiltonianPart(): internal error\n");
				// row i of the ordered product basis
				for (k=hamiltonian.getRowPtr(r);k<hamiltonian.getRowPtr(r+1);k++) {
					
					if (option) alphaPrime = hamiltonian.getCol(k);
					else 	    betaPrime  = hamiltonian.getCol(k);
					size_t j = lrs_.super().permutationInverse(alphaPrime + betaPrime * ns);
					if (j<offset || j>=lrs_.super().partition(m+1)) continue;
					SparseElementType tmp = hamiltonian.getValue(k);
					matrixBlock.pushCol(j-offset);
					matrixBlock.pushValue(tmp);
					counter++;
				}
			}
			matrixBlock.setRow(lrs_.super().partition(m+1)-offset,counter);
		}

		void getReflectedEigs(
				RealType& energyTmp,std::vector<SparseElementType>& tmpVec,
				RealType energyTmp1,const std::vector<SparseElementType>& tmpVec1,
				RealType energyTmp2,const std::vector<SparseElementType>& tmpVec2) const
		{
			reflection_.getReflectedEigs(energyTmp,tmpVec,energyTmp1,tmpVec1,energyTmp2,tmpVec2);
		}

		void setReflectionSymmetry(size_t reflectionSector)
		{
			reflection_.setReflectionSymmetry(reflectionSector);
		}

		void printFullMatrix(const SparseMatrixType& matrix) const
		{
			reflection_.printFullMatrix(matrix);
		}

		void printFullMatrixMathematica(const SparseMatrixType& matrix) const
		{
			reflection_.printFullMatrixMathematica(matrix);
		}

		const LeftRightSuperType& leftRightSuper() const
		{
			return lrs_;
		}

	private:
		int m_;
		const LeftRightSuperType&  lrs_;
		std::vector<std::vector<int> > buffer_;
		std::vector<SparseMatrixType> basis2tc_,basis3tc_;
		std::vector<size_t> alpha_,beta_;
		ReflectionSymmetryType reflection_;
		size_t numberOfOperators_;
		//RightLeftLocalType rightLeftLocal_;
		
		const SparseMatrixType& getTcOperator(int i,size_t type) const
		{
			if (type==System) return basis2tc_[i];
			return basis3tc_[i];
		}
		
		void createBuffer() 
		{
			size_t ns=lrs_.left().size();
			size_t ne=lrs_.right().size();
			int offset = lrs_.super().partition(m_);
			int total = lrs_.super().partition(m_+1) - offset;

			std::vector<int>  tmpBuffer(ne);
			for (size_t alphaPrime=0;alphaPrime<ns;alphaPrime++) {
				for (size_t betaPrime=0;betaPrime<ne;betaPrime++) {	
					tmpBuffer[betaPrime] =lrs_.super().
							permutationInverse(alphaPrime + betaPrime*ns) -
								offset;
					if (tmpBuffer[betaPrime]>=total) tmpBuffer[betaPrime]= -1 ;
				}
				buffer_[alphaPrime]=tmpBuffer;
			}
		}

		void createTcOperators(std::vector<SparseMatrixType>& basistc,
				const BasisWithOperatorsType& basis)
		{
			for (size_t i=0;i<basistc.size();i++)
				transposeConjugate( basistc[i],basis.getOperatorByIndex(i).data);
		}

		void createAlphaAndBeta()
		{
			size_t ns=lrs_.left().size();
			int offset = lrs_.super().partition(m_);
			int total = lrs_.super().partition(m_+1) - offset;

			PackIndicesType pack(ns);
			for (int i=0;i<total;i++) {
				// row i of the ordered product basis
				pack.unpack(alpha_[i],beta_[i],
						lrs_.super().permutation(i+offset));
			}
		}
	}; // class ModelHelperLocal
} // namespace Dmrg
/*@}*/

#endif

