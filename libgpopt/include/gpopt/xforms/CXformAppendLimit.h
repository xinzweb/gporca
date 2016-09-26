//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformAppendLimit.h
//
//	@doc:
//		Split a global limit into pair of local and global limit
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAppendLimit_H
#define GPOPT_CXformAppendLimit_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformAppendLimit
	//
	//	@doc:
	//		Split a global limit into pair of local and global limit
	//
	//---------------------------------------------------------------------------
	class CXformAppendLimit : public CXformExploration
	{

		private:

			// private copy ctor
			CXformAppendLimit(const CXformAppendLimit &);

			// helper function for creating a limit expression
			CExpression *PexprLimit
				(
				IMemoryPool *pmp , // memory pool
				CExpression *pexprRelational, // relational child
				CExpression *pexprScalarStart, // limit offset
				CExpression *pexprScalarRows, // limit count
				COrderSpec *pos, // ordering specification
				BOOL fGlobal, // is it a local or global limit
				BOOL fHasCount, // does limit specify a number of rows
				BOOL fTopLimitUnderDML
				)
			const;

		public:

			// ctor
			explicit
			CXformAppendLimit(IMemoryPool *pmp);

			// dtor
			virtual
			~CXformAppendLimit()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfAppendLimit;
			}

			// return a string for xform name
			virtual
			const CHAR *SzId() const
			{
				return "CXformAppendLimit";
			}

			// Compatibility function for splitting limit
			virtual
			BOOL FCompatible(CXform::EXformId exfid)
			{
				return (CXform::ExfAppendLimit != exfid);
			}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp (CExpressionHandle &exprhdl) const;

			// actual transform
			void Transform
					(
					CXformContext *pxfctxt,
					CXformResult *pxfres,
					CExpression *pexpr
					)
			const;

	}; // class CXformAppendLimit

}

#endif // !GPOPT_CXformAppendLimit_H

// EOF
