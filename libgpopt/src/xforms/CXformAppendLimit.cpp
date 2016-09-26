
//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformAppendLimit.cpp
//
//	@doc:
//		Implementation of the adding limit to unionAll
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformAppendLimit.h"

using namespace gpmd;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformAppendLimit::CXformAppendLimit
//
//	@doc:
//		Ctor
//
//  input
//		limit
//			union all
//				child1
//				child2
//
//	output
//		limit *
//			union all *
//				limit *
//					child1
//				limit *
//					child2
//
//---------------------------------------------------------------------------
CXformAppendLimit::CXformAppendLimit
	(
	IMemoryPool *pmp
	)
	:
	CXformExploration
		(
		 // pattern
		GPOS_NEW(pmp) CExpression
					(
					pmp,
					GPOS_NEW(pmp) CLogicalLimit(pmp),
					GPOS_NEW(pmp) CExpression(pmp,
							GPOS_NEW(pmp) CLogicalUnionAll(pmp),
							GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternMultiLeaf(pmp))
					), // relational child
					GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternLeaf(pmp)),  // scalar child for offset
					GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternLeaf(pmp))  // scalar child for number of rows
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformAppendLimit::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformAppendLimit::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if (0 < exprhdl.Pdprel()->PcrsOuter()->CElements())
	{
		return CXform::ExfpNone;
	}

	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(exprhdl.Pop());
	if (popLimit->FGlobal() || !popLimit->FHasCount())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformAppendLimit::Transform
//
//	@doc:
//		Actual transformation to push limit inside the append
//
//---------------------------------------------------------------------------
void
CXformAppendLimit::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(NULL != pxfres);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	IMemoryPool *pmp = pxfctxt->Pmp();
	// extract components
	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(pexpr->Pop());
	CExpression *pexprUnionAll = (*pexpr)[0];
	CLogicalUnionAll *popUnionAll =  CLogicalUnionAll::PopConvert(pexprUnionAll->Pop());

	CExpression *pexprScalarStart = (*pexpr)[1];
	CExpression *pexprScalarRows = (*pexpr)[2];
	COrderSpec *pos = popLimit->Pos();
	CExpression *child1 = (*pexprUnionAll)[0];
	CExpression *child2 = (*pexprUnionAll)[1];

	// get relational properties for both children
	CDrvdPropRelational *pdprelChild1 =
			CDrvdPropRelational::Pdprel(child1->Pdp(CDrvdProp::EptRelational));

	CDrvdPropRelational *pdprelChild2 =
				CDrvdPropRelational::Pdprel(child2->Pdp(CDrvdProp::EptRelational));

	// TODO: , Feb 20, 2012, we currently only split limit with offset 0.
	if (!CUtils::FHasZeroOffset(pexpr) || (0 < pdprelChild1->PcrsOuter()->CElements() && 0 < pdprelChild2->PcrsOuter()->CElements()) )
	{
		return;
	}

	// addref all components
	child1->AddRef();
	child2->AddRef();

	CExpression *pexprLimitChild1 = PexprLimit
			(
			pmp,
			child1,
			pexprScalarStart,
			pexprScalarRows,
			pos,
			false, // fGlobal
			popLimit->FHasCount(),
			popLimit->FTopLimitUnderDML()
			);

	CExpression *pexprLimitChild2 = PexprLimit
				(
				pmp,
				child2,
				pexprScalarStart,
				pexprScalarRows,
				pos,
				false, // fGlobal
				popLimit->FHasCount(),
				popLimit->FTopLimitUnderDML()
				);


		DrgPexpr *pdrgpexpr = GPOS_NEW(pmp) DrgPexpr(pmp);
		pexprLimitChild1->AddRef();
		pdrgpexpr->Append(pexprLimitChild1);

		pexprLimitChild2->AddRef();
		pdrgpexpr->Append(pexprLimitChild2);

		DrgPcr *pdrgpcrOutput = popUnionAll->PdrgpcrOutput();
		DrgDrgPcr *pdrgpdrgpcrInput = popUnionAll->PdrgpdrgpcrInput();

		pdrgpcrOutput->AddRef();
		pdrgpdrgpcrInput->AddRef();

	// assemble new logical operator
		CExpression *pexprUnionAllWithLimit = GPOS_NEW(pmp) CExpression
										(
										pmp,
										GPOS_NEW(pmp) CLogicalUnionAll(pmp, pdrgpcrOutput, pdrgpdrgpcrInput),
										pdrgpexpr // children of union all
										);

		CExpression *pexprLimitLocalUnionAll = PexprLimit
					(
					pmp,
					pexprUnionAllWithLimit,
					pexprScalarStart,
					pexprScalarRows,
					pos,
					false, // fGlobal
					popLimit->FHasCount(),
					popLimit->FTopLimitUnderDML()
					);

	pxfres->Add(pexprLimitLocalUnionAll);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformAppendLimit::PexprLimit
//
//	@doc:
//		Generate a limit operator
//
//---------------------------------------------------------------------------
CExpression *
CXformAppendLimit::PexprLimit
	(
	IMemoryPool *pmp,
	CExpression *pexprRelational,
	CExpression *pexprScalarStart,
	CExpression *pexprScalarRows,
	COrderSpec *pos,
	BOOL fGlobal,
	BOOL fHasCount,
	BOOL fTopLimitUnderDML
	)
	const
{
	pexprScalarStart->AddRef();
	pexprScalarRows->AddRef();
	pos->AddRef();

	// assemble global limit operator
	CExpression *pexprLimit = GPOS_NEW(pmp) CExpression
			(
			pmp,
			GPOS_NEW(pmp) CLogicalLimit(pmp, pos, fGlobal, fHasCount, fTopLimitUnderDML),
			pexprRelational,
			pexprScalarStart,
			pexprScalarRows
			);

	return pexprLimit;
}

// EOF



