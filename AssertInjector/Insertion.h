#ifndef All_H
#define All_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace std;

class Insertion : public ASTConsumer
{
public:
    Insertion(Rewriter *rewriter, const SourceManager *sourceManager, const ASTContext *astContext, const LangOptions *langOptions );
    virtual bool HandleTopLevelDecl(DeclGroupRef DR) ;
	
	int getCountAssertionDivideByZero();
	int getCountAssertionNullPointerDereference();
	int getCountAssertionArrayOutOfBound();
    int getCountAssertionIntegerOverflow(); 

private:
    Rewriter *m_rewriter;
	const SourceManager *m_sourceManager;
	const ASTContext *m_astContext;
	const LangOptions *m_langOptions;
	
	int m_countAssertionDivideByZero;
	int m_countAssertionNullPointerDereference;
	int m_countAssertionArrayOutOfBound;
    int m_countAssertionIntegerOverflow;
};
#endif
