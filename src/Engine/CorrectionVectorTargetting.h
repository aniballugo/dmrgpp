
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

/*! \file CorrectionVectorTargetting.h
 *
 * Implements the targetting required by
 * the correction targetting method
 *
 */

#ifndef CORRECTION_VECTOR_TARG_H
#define CORRECTION_VECTOR_TARG_H

#include "ProgressIndicator.h"
#include "BLAS.h"
#include "ApplyOperatorLocal.h"
#include "DynamicSerializer.h"
#include "CorrectionVectorParams.h"
#include "VectorWithOffsets.h"
#include "ContinuedFraction.h"
#include "CorrectionVectorFunction.h"

namespace Dmrg {
	
	template<
		template<typename,typename,typename> class LanczosSolverTemplate,
		template<typename,typename> class InternalProductTemplate,
		template<typename,typename> class WaveFunctionTransfTemplate,
		typename ModelType_,
		typename ConcurrencyType_,
		typename IoType_,
		template<typename> class VectorWithOffsetTemplate>
	class CorrectionVectorTargetting  {
	public:
		
		typedef ModelType_ ModelType;
		typedef ConcurrencyType_ ConcurrencyType;
		typedef IoType_ IoType;
		typedef typename ModelType::RealType RealType;
		typedef std::complex<RealType> ComplexType;
		typedef InternalProductTemplate<RealType,ModelType> InternalProductType;
		typedef typename ModelType::OperatorsType OperatorsType;
		typedef typename ModelType::ModelHelperType ModelHelperType;
		typedef typename ModelHelperType::LeftRightSuperType
			LeftRightSuperType;
		typedef typename LeftRightSuperType::BasisWithOperatorsType BasisWithOperatorsType;
		typedef typename BasisWithOperatorsType::OperatorType OperatorType;
		typedef typename BasisWithOperatorsType::BasisType BasisType;
		typedef CorrectionVectorParams<ModelType> TargettingParamsType;
		typedef typename BasisType::BlockType BlockType;
		typedef VectorWithOffsetTemplate<RealType> VectorWithOffsetType;
		typedef typename VectorWithOffsetType::VectorType VectorType;
		typedef LanczosSolverTemplate<RealType,InternalProductType,VectorType> LanczosSolverType;
		typedef VectorType TargetVectorType;
		typedef ApplyOperatorLocal<LeftRightSuperType,VectorWithOffsetType,TargetVectorType> ApplyOperatorType;
		typedef TimeSerializer<RealType,VectorWithOffsetType> TimeSerializerType;
		typedef WaveFunctionTransfTemplate<LeftRightSuperType,VectorWithOffsetType> WaveFunctionTransfType;
		typedef typename LanczosSolverType::TridiagonalMatrixType TridiagonalMatrixType;
		typedef typename LanczosSolverType::DenseMatrixType DenseMatrixType;
		typedef PsimagLite::ContinuedFraction<RealType,TridiagonalMatrixType>
			ContinuedFractionType;
		typedef DynamicSerializer<RealType,VectorWithOffsetType,
				ContinuedFractionType> DynamicSerializerType;
		typedef typename LanczosSolverType::LanczosMatrixType
		                                      LanczosMatrixType;
		typedef CorrectionVectorFunction<RealType,
		                                  LanczosMatrixType,
		                                  TargettingParamsType>
		         CorrectionVectorFunctionType;
		
		enum {DISABLED,OPERATOR,CONVERGING};
		enum {	EXPAND_ENVIRON=WaveFunctionTransfType::EXPAND_ENVIRON,
				EXPAND_SYSTEM=WaveFunctionTransfType::EXPAND_SYSTEM,
				INFINITE=WaveFunctionTransfType::INFINITE};
		static size_t const PRODUCT = TargettingParamsType::PRODUCT;
		static size_t const SUM = TargettingParamsType::SUM;

		static const size_t parallelRank_ = 0; // DYNT needs to support concurrency FIXME

		CorrectionVectorTargetting(
				const LeftRightSuperType& lrs,
				const ModelType& model,
				const TargettingParamsType& tstStruct,
				const WaveFunctionTransfType& wft)
		:	
		 	stage_(tstStruct.sites.size(),DISABLED),
		 	lrs_(lrs),
		 	model_(model),
		 	tstStruct_(tstStruct),
		 	waveFunctionTransformation_(wft),
		 	progress_("CorrectionVectorTargetting",0),
		 	applyOpLocal_(lrs),
		 	gsWeight_(1.0),
		 	targetVectors_(3)
		{
			if (!wft.isEnabled()) throw std::runtime_error(" CorrectionVectorTargetting "
					"needs an enabled wft\n");
		}

		RealType weight(size_t i) const
		{
			if (allStages(DISABLED)) throw std::runtime_error(
					"DynTarget: What are you doing here?\n");
			return weight_[i];
			//return 1.0;
		}

		RealType gsWeight() const
		{
			if (allStages(DISABLED)) return 1.0;
			return gsWeight_;
		}
		
		RealType normSquared(size_t i) const
		{
			// call to mult will conjugate one of the vector
			return std::real(multiply(targetVectors_[i],targetVectors_[i]));
		}
		
		template<typename SomeBasisType>
		void setGs(const std::vector<TargetVectorType>& v,
				const SomeBasisType& someBasis)
		{
			psi_.set(v,someBasis);
		}
		
		const RealType& operator[](size_t i) const { return psi_[i]; }
					
		RealType& operator[](size_t i) { return psi_[i]; }
		
		const VectorWithOffsetType& gs() const { return psi_; }
		
		bool includeGroundStage() const {return true; }
		
		size_t size() const
		{
			if (allStages(DISABLED)) return 0;
			return targetVectors_.size();
		}
		
		const VectorWithOffsetType& operator()(size_t i) const
		{
			return targetVectors_[i];
		}
		
		void evolve(RealType Eg,size_t direction,const BlockType& block,
					size_t loopNumber)
		{
			if (block.size()!=1) throw
					std::runtime_error("CorrectionVectorTargetting::evolve(...):"
							" blocks of size != 1 are unsupported (sorry)\n");
			size_t site = block[0];
			evolve(Eg,direction,site,loopNumber);
			size_t numberOfSites = lrs_.super().block().size();
			if (site>1 && site<numberOfSites-2) return;
			// //corner case
			size_t x = (site==1) ? 0 : numberOfSites-1;
			evolve(Eg,direction,x,loopNumber);
		}

		void evolve(RealType Eg,size_t direction,size_t site,
				size_t loopNumber)
		{
			size_t count =0;
			VectorWithOffsetType phiOld = psi_;
			VectorWithOffsetType phiNew;
			VectorWithOffsetType vectorSum;

			size_t max = tstStruct_.sites.size();

			if (noStageIs(DISABLED)) max = 1;

			// Loop over each operator that needs to be applied
			// in turn to the g.s.
			for (size_t i=0;i<max;i++) {
				count += evolve(i,phiNew,phiOld,Eg,direction,site,loopNumber,max-1);
				if (tstStruct_.concatenation==PRODUCT) {
					phiOld = phiNew;
				} else {
					vectorSum += phiNew;
				}
			}
			if (tstStruct_.concatenation==SUM) phiNew = vectorSum;

			if (count==0) {
		//		// always print to keep observer driver in sync
		//		if (needsPrinting) {
		//			zeroOutVectors();
		//			printVectors(block);
		//		}
				return;
			}
			if (!noStageIs(DISABLED)) Eg_ = Eg;
			calcDynVectors(phiNew,direction);

			//cocoon(direction,block); // in-situ

			//if (needsPrinting) printVectors(block); // for post-processing
		}
		

		void initialGuess(VectorWithOffsetType& v) const
		{
			waveFunctionTransformation_.setInitialVector(v,psi_,lrs_);
			if (!allStages(CONVERGING)) return;
			std::vector<VectorWithOffsetType> vv(targetVectors_.size());
			for (size_t i=0;i<targetVectors_.size();i++) {
				waveFunctionTransformation_.setInitialVector(vv[i],
						targetVectors_[i],lrs_);
				if (std::norm(vv[i])<1e-6) continue;
				VectorWithOffsetType w= weight_[i]*vv[i];
				v += w;
			}
			if (fabs(gsWeight_)<1e-6) return;
			VectorWithOffsetType w= gsWeight_*psi_;
			v += w;
		}
		
		const LeftRightSuperType& leftRightSuper() const { return lrs_; }

		template<typename IoOutputType>
		void save(const std::vector<size_t>& block,IoOutputType& io) const
		{
			if (block.size()!=1) throw std::runtime_error(
					"CorrectionVectorTargetting only supports blocks of size 1\n");
			size_t type = tstStruct_.type;
			int s = (type&1) ? -1 : 1;
			int s2 = (type>1) ? -1 : 1;
			ContinuedFractionType cf(ab_,Eg_,s2*weightForContinuedFraction_,
				s);
			DynamicSerializerType dynS(cf,block[0],targetVectors_);
			dynS.save(io);
			psi_.save(io,"PSI");
		}

		void load(const std::string& f)
		{
			for (size_t i=0;i<stage_.size();i++) stage_[i] = CONVERGING;

			typename IoType::In io(f);

			DynamicSerializerType dynS(io,IoType::In::LAST_INSTANCE);
			for (size_t i=0;i<targetVectors_.size();i++)
				targetVectors_[i] = dynS.vector(i);

			psi_.load(io,"PSI");
		}

	private:

		size_t evolve(
				size_t i,
				VectorWithOffsetType& phiNew,
				VectorWithOffsetType& phiOld,
				RealType Eg,
				size_t direction,
				size_t site,
				size_t loopNumber,
				size_t lastI)
		{
			if (tstStruct_.startingLoops[i]>loopNumber || direction==INFINITE) return 0;
			
			if (site != tstStruct_.sites[i] && stage_[i]==DISABLED) return 0;
			
			if (site == tstStruct_.sites[i] && stage_[i]==DISABLED) stage_[i]=OPERATOR;
			else stage_[i]=CONVERGING;
			if (stage_[i] == OPERATOR) checkOrder(i);
			
			std::ostringstream msg;
			msg<<"Evolving, stage="<<getStage(i)<<" site="<<site<<" loopNumber="<<loopNumber;
			msg<<" Eg="<<Eg;
			progress_.printline(msg,std::cout);
							
			// phi = A|psi>
			computePhi(i,site,phiNew,phiOld,direction);
			
			return 1;
		}

		void computePhi(
				size_t i,
				size_t site,
				VectorWithOffsetType& phiNew,
				VectorWithOffsetType& phiOld,
				size_t systemOrEnviron)
		{
			size_t numberOfSites = lrs_.super().block().size();
			if (stage_[i]==OPERATOR) {

				bool corner = (tstStruct_.sites[i]==0 ||
						tstStruct_.sites[i]==numberOfSites -1) ? true : false;

				std::ostringstream msg;
				msg<<"I'm applying a local operator now";
				progress_.printline(msg,std::cout);
				FermionSign fs(lrs_.left(),tstStruct_.electrons);
				applyOpLocal_(phiNew,phiOld,tstStruct_.aOperators[i],
						fs,systemOrEnviron,corner);
				RealType norma = std::norm(phiNew);
				if (norma==0) throw std::runtime_error(
						"Norm of phi is zero\n");
				//std::cerr<<"Norm of phi="<<norma<<" when i="<<i<<"\n";
				
			} else if (stage_[i]== CONVERGING) {
				if (site==0 || site==numberOfSites -1)  {
					// don't wft since we did it before
					phiNew = targetVectors_[0];
					return;
				}
				std::ostringstream msg;
				msg<<"I'm calling the WFT now";
				progress_.printline(msg,std::cout);

				//if (tstStruct_.aOperators.size()==1)
				//	guessPhiSectors(phiNew,i,systemOrEnviron);
				//else
					phiNew.populateSectors(lrs_.super());

				// OK, now that we got the partition number right, let's wft:
				waveFunctionTransformation_.setInitialVector(
						phiNew,targetVectors_[0],lrs_); // generalize for su(2)
				phiNew.collapseSectors();
				
			} else {
				throw std::runtime_error("It's 5 am, do you know what line "
					" your code is exec-ing?\n");
			}
		}

		void checkOrder(size_t i) const
		{
			if (i==0) return;
			for (size_t j=0;j<i;j++) {
				if (stage_[j] == DISABLED) {
					std::string s ="TST:: Seeing dynamic site "+ttos(tstStruct_.sites[i]);
					s =s + " before having seen";
					s = s + " site "+ttos(j);
					s = s +". Please order your dynamic sites in order of appearance.\n";
					throw std::runtime_error(s);
				}
			}
		}

		bool allStages(size_t x) const
		{
			for (size_t i=0;i<stage_.size();i++)
				if (stage_[i]!=x) return false;
			return true;
		}
		

		bool noStageIs(size_t x) const
		{
			for (size_t i=0;i<stage_.size();i++)
				if (stage_[i]==x) return false;
			return true;
		}
		

		std::string getStage(size_t i) const
		{
			switch (stage_[i]) {
			case DISABLED:
				return "Disabled";
				break;
			case OPERATOR:
				return "Applying operator for the first time";
				break;
			case CONVERGING:
				return "Converging DDMRG";
				break;
			}
			return "undefined";
		}
		
		void calcDynVectors(
				const VectorWithOffsetType& phi,
				size_t systemOrEnviron)
		{
			for (size_t i=0;i<targetVectors_.size();i++)
				targetVectors_[i] = phi;

			for (size_t i=0;i<phi.sectors();i++) {
				VectorType sv;
				size_t i0 = phi.sector(i);
				phi.extract(sv,i0);
				// g.s. is included separately
				// set Aq
				targetVectors_[0].setDataInSector(sv,i0);
				// set xi
				size_t p = lrs_.super().findPartitionNumber(phi.offset(i0));
				VectorType xi(sv.size(),0),xr(sv.size(),0);
				computeXiAndXr(xi,xr,sv,p);
				targetVectors_[1].setDataInSector(xi,i0);
				//set xr
				targetVectors_[2].setDataInSector(xr,i0);
				DenseMatrixType V;
				getLanczosVectors(V,sv,p);
			}
			setWeights();
			weightForContinuedFraction_ = phi*phi;
		}

		void getLanczosVectors(
				DenseMatrixType& V,
				const VectorType& sv,
				size_t p)
		{
			typename ModelType::ModelHelperType modelHelper(
					p,lrs_,model_.orbitals());
			typedef typename LanczosSolverType::LanczosMatrixType
					LanczosMatrixType;
			LanczosMatrixType h(&model_,&modelHelper);

			RealType eps= 0.01*ProgramGlobals::LanczosTolerance;
			size_t iter= ProgramGlobals::LanczosSteps;

			//srand48(3243447);
			LanczosSolverType lanczosSolver(h,iter,eps,parallelRank_);

			lanczosSolver.tridiagonalDecomposition(sv,ab_,V);
			//calcIntensity(Eg,sv,V,ab);
		}

		void computeXiAndXr(VectorType& xi,
		                      VectorType& xr,
		                      const VectorType& sv,
		                      size_t p)
		{
			typename ModelType::ModelHelperType modelHelper(
					p,lrs_,model_.orbitals());
			LanczosMatrixType h(&model_,&modelHelper);
			CorrectionVectorFunctionType cvft(h,tstStruct_);
			cvft.getXi(xi,sv);
			// make sure xr is zero
			for (size_t i=0;i<xr.size();i++) xr[i] = 0;
			h.matrixVectorProduct(xr,xi);
			xr -= tstStruct_.omega*xi;
			xr /= tstStruct_.eta;
		}

		void guessPhiSectors(VectorWithOffsetType& phi,size_t i,size_t systemOrEnviron)
		{
			FermionSign fs(lrs_.left(),tstStruct_.electrons);
			if (allStages(CONVERGING)) {
				VectorWithOffsetType tmpVector = psi_;
				for (size_t j=0;j<tstStruct_.aOperators.size();j++) {
					applyOpLocal_(phi,tmpVector,tstStruct_.aOperators[j],fs,
							systemOrEnviron);
					tmpVector = phi;
				}
				return;
			}
			applyOpLocal_(phi,psi_,tstStruct_.aOperators[i],fs,
					systemOrEnviron);
		}
		
		void setWeights()
		{
			RealType sum  = 0;
			weight_.resize(targetVectors_.size());
			for (size_t r=0;r<weight_.size();r++) {
				weight_[r] =0;
				for (size_t i=0;i<targetVectors_[0].sectors();i++) {
					VectorType v,w;
					size_t i0 = targetVectors_[0].sector(i);
					targetVectors_[0].extract(v,i0);
					targetVectors_[r].extract(w,i0);
					weight_[r] += dynWeightOf(v,w);
				}
				sum += weight_[r];
			}
			for (size_t r=0;r<weight_.size();r++) weight_[r] *= 0.5/sum;
			gsWeight_ = 0.5;

		}

		RealType dynWeightOf(VectorType& v,const VectorType& w) const
		{
			RealType sum = 0;
			for (size_t i=0;i<v.size();i++) {
				RealType tmp = std::real(v[i]*w[i]);
				sum += tmp*tmp;
			}
			return sum;
		}

		void zeroOutVectors()
		{
			for (size_t i=0;i<targetVectors_.size();i++)
				targetVectors_[i].resize(lrs_.super().size());
		}
		

		//void printHeader()
		//{
		//	io_.print(tstStruct_);
		//	std::string label = "omega";
		//	std::string s = "Omega=" + ttos(currentOmega_);
		//	io_.printline(s);
		//	label = "weights";
		//	io_.printVector(weight_,label);
		//	s = "GsWeight="+ttos(gsWeight_);
		//	io_.printline(s);
		//}
		
		std::vector<size_t> stage_;
		VectorWithOffsetType psi_;
		const LeftRightSuperType& lrs_;
		const ModelType& model_;
		const TargettingParamsType& tstStruct_;
		const WaveFunctionTransfType& waveFunctionTransformation_;
		PsimagLite::ProgressIndicator progress_;
		ApplyOperatorType applyOpLocal_;
		RealType gsWeight_;
		std::vector<VectorWithOffsetType> targetVectors_;
		std::vector<RealType> weight_;
		TridiagonalMatrixType ab_;
		RealType Eg_;
		RealType weightForContinuedFraction_;
		//typename IoType::Out io_;
		

	}; // class CorrectionVectorTargetting
	
	template<
	template<typename,typename,typename> class LanczosSolverTemplate,
	template<typename,typename> class InternalProductTemplate,
	template<typename,typename> class WaveFunctionTransfTemplate,
	typename ModelType_,
	typename ConcurrencyType_,
	typename IoType_,
	template<typename> class VectorWithOffsetTemplate>
	std::ostream& operator<<(std::ostream& os,
			const CorrectionVectorTargetting<LanczosSolverTemplate,
			InternalProductTemplate,
			WaveFunctionTransfTemplate,ModelType_,ConcurrencyType_,IoType_,
			VectorWithOffsetTemplate>& tst)
	{
		os<<"DT=NothingToSeeHereYet\n";
		return os;
	}
	
} // namespace
/*@}*/
#endif // CORRECTION_VECTOR_TARG_H

