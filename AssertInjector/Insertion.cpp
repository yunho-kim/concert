#include <cstdio>
#include <string>
#include <sstream>

#include "Insertion.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

struct stackframe {
	Stmt *stmt;
	StmtRange progress;
	bool isAssertInsertionLevel;
	string assertFromChildren;
};

class RecursiveASTTraverse {
public:
	RecursiveASTTraverse(Rewriter *rewriter, const SourceManager *sourceManager, const ASTContext *astContext, const LangOptions *langOptions, Decl *funcDecl);
    string visitStmt(vector<stackframe> history, Stmt *s);
	
	string checkNullPointerDereference(vector<stackframe> history, DeclRefExpr *s);
	string checkArrayOutOfBound(vector<stackframe> history, ArraySubscriptExpr *s);
	string checkDivideByZero(vector<stackframe> history, BinaryOperator *s);
    string checkIntegerOverflow(vector<stackframe> history, BinaryOperator *s);
	
	string childInIfStmt(vector<stackframe> history, Stmt *current);

	int getCountAssertionDivideByZero() { return m_countAssertionDivideByZero; };
	int getCountAssertionNullPointerDereference() { return m_countAssertionNullPointerDereference; };
	int getCountAssertionArrayOutOfBound() { return m_countAssertionArrayOutOfBound; };
    int getCountAssertionIntegerOverflow() { return m_countAssertionIntegerOverflow; };
	
	
private:
	bool traverseFunctionDecl(Decl *decl);
	string traverseStmtRecursive(vector<stackframe> history);
	
	string getAssertStatement(string condition, string comment);
	bool hasSideEffects(Expr *expr);
	SourceLocation getEndOfStmt(Stmt *s);
	string removeDuplicatedLines(string input);
	
	Rewriter *m_rewriter;
	const SourceManager *m_sourceManager;	
	const ASTContext *m_astContext;
	const LangOptions *m_langOptions;
	
	int m_countAssertionDivideByZero;
	int m_countAssertionNullPointerDereference;
	int m_countAssertionArrayOutOfBound;
    int m_countAssertionIntegerOverflow;
};

RecursiveASTTraverse::RecursiveASTTraverse(Rewriter* rewriter, const SourceManager* sourceManager, const ASTContext *astContext, const LangOptions *langOptions, Decl* funcDecl) 
	: m_rewriter(rewriter), m_sourceManager(sourceManager), m_astContext(astContext), m_langOptions(langOptions)
	, m_countAssertionDivideByZero(0), m_countAssertionNullPointerDereference(0), m_countAssertionArrayOutOfBound(0)
    , m_countAssertionIntegerOverflow(0)
{
	traverseFunctionDecl(funcDecl);
}

bool RecursiveASTTraverse::traverseFunctionDecl(Decl *decl) {
	Stmt *bodyStmt = decl->getBody();
		
	vector<stackframe> history;
	stackframe newRootFrame;
	newRootFrame.stmt = bodyStmt;
	newRootFrame.progress = bodyStmt->children();
	newRootFrame.isAssertInsertionLevel = true;
	newRootFrame.assertFromChildren = "";
	history.push_back(newRootFrame);
	//bodyStmt->dumpColor();
	traverseStmtRecursive(history);
	
	return true;
}

string RecursiveASTTraverse::traverseStmtRecursive(vector<stackframe> history) {
	
	string resultAsserts = "";
	for(stackframe currentFrame = history.back();!currentFrame.progress.empty();currentFrame.progress++) {
		if( *currentFrame.progress == NULL ) 
			continue;
			
		Stmt *currentChild = *(currentFrame.progress);
		string asserts = "";
		
		if( !currentChild->children().empty() ) {
			stackframe newFrame;
			newFrame.stmt = currentChild;
			newFrame.progress = currentChild->children();
			newFrame.isAssertInsertionLevel = false;
			
			if(isa<IfStmt>(currentFrame.stmt)) {
				asserts += childInIfStmt(history, currentChild);
			} else if(isa<ForStmt>(currentFrame.stmt)) {
				ForStmt* forStmt = cast<ForStmt>(currentFrame.stmt);
				if( forStmt->getBody() == currentChild ) {
						
					if( isa<CompoundStmt>(currentChild) ) {
						newFrame.isAssertInsertionLevel = true;
						history.push_back(newFrame);
						traverseStmtRecursive(history);
						history.pop_back();
						
					} else {
						history.push_back(newFrame);
						asserts += traverseStmtRecursive(history);
                        if ( !asserts.empty() ) {
							m_rewriter->InsertText( getEndOfStmt(currentChild) , "}" , true, true);
							asserts = "";
						}
					}
				}
			} else if(isa<WhileStmt>(currentFrame.stmt)) {
				WhileStmt* whileStmt = cast<WhileStmt>(currentFrame.stmt);
				if( whileStmt->getCond() == currentChild ) {
					history.push_back(newFrame);
					asserts += traverseStmtRecursive(history);
					history.pop_back();
				} else if( whileStmt->getBody() == currentChild ) {
					if( isa<CompoundStmt>(currentChild) ) {
						newFrame.isAssertInsertionLevel = true;
						history.push_back(newFrame);
						asserts += traverseStmtRecursive(history);
						history.pop_back();
					} else {
						history.push_back(newFrame);
						asserts += traverseStmtRecursive(history);
						history.pop_back();
						
						if( !asserts.empty() ) {
							m_rewriter->InsertText( currentChild->getLocStart() , "{"+removeDuplicatedLines(asserts) , true, true);		
							m_rewriter->InsertText( getEndOfStmt(currentChild) , "}" , true, true);
							asserts = "";
						}
					}
				}
			} else if(isa<DoStmt>(currentFrame.stmt)) {
				DoStmt* doStmt = cast<DoStmt>(currentFrame.stmt);
								
				if( doStmt->getBody() == currentChild ) {
					string conditionAsserts;
					if( doStmt->getCond() ) {
						stackframe conditionFrame;
						conditionFrame.stmt = doStmt->getCond();
						conditionFrame.progress = doStmt->getCond()->children();
						conditionFrame.isAssertInsertionLevel = false;
						history.push_back(conditionFrame);
						conditionAsserts = traverseStmtRecursive(history);
						history.pop_back();
					}
					
					if( isa<CompoundStmt>(currentChild) ) {
						newFrame.isAssertInsertionLevel = true;
						history.push_back(newFrame);
						asserts += traverseStmtRecursive(history);
						history.pop_back();
						
						if( !conditionAsserts.empty() ) {
							m_rewriter->InsertText( currentChild->getLocEnd().getLocWithOffset(-1) , removeDuplicatedLines(conditionAsserts) , true, true);		
						}
					} else {
						history.push_back(newFrame);
						asserts += traverseStmtRecursive(history);
						history.pop_back();
						
						if( !asserts.empty() ) {
							m_rewriter->InsertText( currentChild->getLocStart() , "{"+removeDuplicatedLines(asserts) , true, true);		
							if( !conditionAsserts.empty() ) {
								m_rewriter->InsertText( getEndOfStmt(currentChild) , removeDuplicatedLines(conditionAsserts) , true, true);		
							}							
							m_rewriter->InsertText( getEndOfStmt(currentChild) , "}" , true, true);
							asserts = "";
						}
					}
				}
			} else {
				if( isa<CompoundStmt>(currentChild) || isa<DefaultStmt>(currentChild) ) {
					newFrame.isAssertInsertionLevel = true;
				}
				history.push_back(newFrame);
				asserts += traverseStmtRecursive(history);
				history.pop_back();
			}
		}
		
		asserts += visitStmt(history, currentChild);
		
		
		if( !asserts.empty() ) {
			if( currentFrame.isAssertInsertionLevel ) {
				m_rewriter->InsertText( currentChild->getLocStart() , removeDuplicatedLines(asserts) , true, true);		
			} else {
				resultAsserts += asserts;
			}
		}
	}
	return resultAsserts;
}

string RecursiveASTTraverse::visitStmt(vector<stackframe> history, Stmt *s) {
	stringstream assertResult;
	if(isa<BinaryOperator>(s) ) {
		assertResult << checkDivideByZero(history, cast<BinaryOperator>(s));
        assertResult << checkIntegerOverflow(history, dyn_cast<BinaryOperator>(s));
	} else if(isa<ArraySubscriptExpr>(s) ) {
		assertResult << checkArrayOutOfBound(history, cast<ArraySubscriptExpr>(s));
	} else if(isa<DeclRefExpr>(s) ) {
		assertResult << checkNullPointerDereference(history, cast<DeclRefExpr>(s) );
	}
	return assertResult.str() ;
}

string RecursiveASTTraverse::checkNullPointerDereference(vector<stackframe> history, DeclRefExpr *s) {
	stringstream assertResult;
	if( s->getType()->isAnyPointerType () && isa<ImplicitCastExpr>(history.back().stmt) ) {
		if( isa<UnaryOperator>(history.at(history.size()-2).stmt) ) {
			UnaryOperator* unaryOperator = cast<UnaryOperator>(history.at(history.size()-2).stmt);
			if(unaryOperator->getOpcode() == UO_Deref && !s->isEvaluatable(*m_astContext) ) {
                bool isInSizeOf = false;
                for(int i = history.size()-3; i >= 0 ; i--) {
                    if ( isa<UnaryExprOrTypeTraitExpr>(history.at(i).stmt) && (cast<UnaryExprOrTypeTraitExpr>(history.at(i).stmt))->getKind() == UETT_SizeOf ) {
                        assertResult << "/* NullPointerDereference : skip deref in sizeof */\n";
                        isInSizeOf = true;
                        break;
                    }
                }
                
                if(!isInSizeOf) {
                    if( !hasSideEffects( s ) ) {
					    assertResult << getAssertStatement( "0==("+m_rewriter->ConvertToString(s)+")", "NullPointerDereference");			
    					m_countAssertionNullPointerDereference++;
	    			} else {
		    			assertResult << "/* NullPointerDereference : skip due to side effect */\n";
			    	}
                }
			}
		} else if( isa<MemberExpr >(history.at(history.size()-2).stmt) ) {
			if( !hasSideEffects( s ) ) {
				assertResult << getAssertStatement( "0==("+m_rewriter->ConvertToString(s)+")", "NullPointerDereference");			
				m_countAssertionNullPointerDereference++;
			} else {
				assertResult << "/* NullPointerDereference : skip due to side effect */\n";
			}
		} else if( isa<ArraySubscriptExpr >(history.at(history.size()-2).stmt) ) {
			if( !hasSideEffects( s ) ) {
				assertResult << getAssertStatement( "0==("+m_rewriter->ConvertToString(s)+")", "NullPointerDereference");			
				m_countAssertionNullPointerDereference++;
			} else {
				assertResult << "/* NullPointerDereference : skip due to side effect */\n";
			}
		}
	}
	return assertResult.str();
}

string RecursiveASTTraverse::checkArrayOutOfBound(vector<stackframe> history, ArraySubscriptExpr *s) {
	stringstream assertResult;			
	StmtRange  arraySubscript_child( s->children() );
	
	if( arraySubscript_child && isa<ImplicitCastExpr>(*arraySubscript_child) ) {
		StmtRange implicitCaseExpr_child( arraySubscript_child->children() );
		arraySubscript_child++;
		Expr *index = cast<Expr>(*arraySubscript_child);
		if( implicitCaseExpr_child && isa<DeclRefExpr>(*implicitCaseExpr_child) && !index->isEvaluatable(*m_astContext) ) {
			DeclRefExpr *arrayImplicitCastExpr = cast<DeclRefExpr>(*implicitCaseExpr_child);
			QualType exprType = arrayImplicitCastExpr->getType();
			
			if( exprType->isArrayType() ) {
				const ArrayType* arrayType = exprType->getAsArrayTypeUnsafe();
				
				if( !arrayType->isIncompleteArrayType() && arrayType->isConstantSizeType() ) {
					if( !hasSideEffects( s->getIdx() ) ) {
						const ConstantArrayType  *constantArray = cast<ConstantArrayType>(arrayType);
						
						assertResult << getAssertStatement( "(" + m_rewriter->ConvertToString(s->getIdx()) + ")<0" , "ArrayOutOfBound");
						
						stringstream conditions;
						conditions<<"("<<constantArray->getSize().getLimitedValue()<<")<=("<<m_rewriter->ConvertToString(s->getIdx())<<")";
						assertResult << getAssertStatement( conditions.str() , "ArrayOutOfBound" );
						m_countAssertionArrayOutOfBound+=2;
					} else {
						assertResult << "/* ArrayOutOfBound : skip due to side effect */\n";
					}
				} 
			}
		}
	}
	
	return assertResult.str();
}

string RecursiveASTTraverse::checkDivideByZero(vector<stackframe> history, BinaryOperator *s) {
	stringstream assertResult;
	if( (s->getOpcode() == BO_Div || s->getOpcode() == BO_Rem || s->getOpcode() == BO_DivAssign || s->getOpcode() == BO_RemAssign) &&
		!s->getRHS()->isEvaluatable(*m_astContext) ) {
		if( !hasSideEffects( s->getRHS() ) ) {
			assertResult << getAssertStatement( "0==" + m_rewriter->ConvertToString(s->getRHS()), "DivideByZero" );
			m_countAssertionDivideByZero++;
		} else {
			assertResult << "/* DivideByZero : skip due to side effect */\n";
		}
	}
	return assertResult.str();
}

string RecursiveASTTraverse::checkIntegerOverflow(vector<stackframe> history, BinaryOperator *s) {

    stringstream assertResult;
    if( (s->getOpcode() == BO_Add) &&
        !s->getRHS()->isEvaluatable(*m_astContext) ) {
        if( !hasSideEffects( s->getLHS() ) && !hasSideEffects( s->getRHS() ) ) {
            string LHS = m_rewriter->ConvertToString(s->getLHS());
            string RHS = m_rewriter->ConvertToString(s->getRHS());
            assertResult << getAssertStatement( "(" + LHS + " > 0 && " + RHS + " > 0 " + ")", "IntegerOverflow" );
            m_countAssertionIntegerOverflow++;
        } else {
            assertResult << "/* IntegerOverflow : skip due to side effect */\n";
        }
    }
    return assertResult.str();

}

string RecursiveASTTraverse::childInIfStmt(vector<stackframe> history, Stmt *child) {
	stackframe newFrame;
	newFrame.stmt = child;
	newFrame.progress = child->children();
	newFrame.isAssertInsertionLevel = false;
	IfStmt* ifStmt = cast<IfStmt>(history.back().stmt);
	string asserts;
	
	if( ifStmt->getCond() == child ) {
		history.push_back(newFrame);
		asserts = traverseStmtRecursive(history);
		history.pop_back();
	} else if ( isa<CompoundStmt>(child) ){
		newFrame.isAssertInsertionLevel = true;
		history.push_back(newFrame);
		traverseStmtRecursive(history);
		history.pop_back();
	} else if ( ifStmt->getThen() == child || ifStmt->getElse() == child ) {
		history.push_back(newFrame);
		asserts = traverseStmtRecursive(history);
		history.pop_back();
		
		if( !asserts.empty() ) {
			m_rewriter->InsertText( child->getLocStart() , "{"+removeDuplicatedLines(asserts) , true, true);		
			m_rewriter->InsertText( getEndOfStmt(child) , "}" , true, true);
			asserts = "";
		}
	} 	
	return asserts;
}

string RecursiveASTTraverse::getAssertStatement(string condition, string comment) {
	stringstream assertResult;
	assertResult << "{extern struct _IO_FILE *stderr;extern int fprintf (struct _IO_FILE *__restrict __stream, __const char *__restrict __format, ...);fprintf(stderr,\"" << condition << ", " << comment << ", %s, %d, %s\\n\",__FILE__, __LINE__, __FUNCTION__); if(" << condition << ") __assert_fail(\"" << condition << "[" << comment << "]" <<"\", __FILE__, __LINE__, __FUNCTION__);/*"<< comment << "*/}\n";
	return assertResult.str();
}

bool RecursiveASTTraverse::hasSideEffects(Expr *expr) {
	return expr->HasSideEffects(*m_astContext);
}

SourceLocation RecursiveASTTraverse::getEndOfStmt(Stmt *s) {
	SourceLocation EndOfStmt = s->getLocEnd();
	bool IsInvalid = false;
	const char *EndOfStmtStr = m_sourceManager->getCharacterData(EndOfStmt, &IsInvalid);
	if (IsInvalid){
		return EndOfStmt.getLocWithOffset(1);
	}
	if (EndOfStmtStr[0] == '}'){
		return EndOfStmt.getLocWithOffset(1);
	}else{
		return clang::Lexer::findLocationAfterToken(EndOfStmt, clang::tok::semi, *m_sourceManager, *m_langOptions, false);
	}
}

string RecursiveASTTraverse::removeDuplicatedLines(string input) {
	stringstream assertStrings( input );
	string noDuplication = "";
	char buffer[1000];
	while ( assertStrings.getline(buffer, 1000,'\n') ) {
		if( noDuplication.find( buffer ) == string::npos ) {
			noDuplication.append(buffer);
			noDuplication.append("\n");
		} else {
			string duplicatedLine(buffer);
			if( duplicatedLine.find( "skip" ) == string::npos) {
				if( duplicatedLine.find( "DivideByZero" ) != string::npos ) {
					m_countAssertionDivideByZero--;
				} else if( duplicatedLine.find( "ArrayOutOfBound" ) != string::npos ) {
					m_countAssertionArrayOutOfBound--;
				} else if( duplicatedLine.find( "NullPointerDereference" ) != string::npos ) {
					m_countAssertionNullPointerDereference--;
				} 
			}
		}
	}
	return noDuplication;
}

Insertion::Insertion(Rewriter* rewriter, const SourceManager* sourceManager, const ASTContext *astContext, const LangOptions *langOptions)
	: m_rewriter(rewriter), m_sourceManager(sourceManager), m_astContext(astContext), m_langOptions(langOptions)
	, m_countAssertionDivideByZero(0), m_countAssertionNullPointerDereference(0), m_countAssertionArrayOutOfBound(0)
{}

bool Insertion::HandleTopLevelDecl(DeclGroupRef DR) {
	for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
		if( (*b)->isFunctionOrFunctionTemplate() && (*b)->hasBody() ) {
			RecursiveASTTraverse traverse(m_rewriter, m_sourceManager, m_astContext, m_langOptions, *b);
			m_countAssertionDivideByZero += traverse.getCountAssertionDivideByZero();
			m_countAssertionNullPointerDereference += traverse.getCountAssertionNullPointerDereference();
			m_countAssertionArrayOutOfBound += traverse.getCountAssertionArrayOutOfBound();
            m_countAssertionIntegerOverflow += traverse.getCountAssertionIntegerOverflow();
		}
	}
	return true;
}

int Insertion::getCountAssertionDivideByZero() { return m_countAssertionDivideByZero; }
int Insertion::getCountAssertionNullPointerDereference() { return m_countAssertionNullPointerDereference; }
int Insertion::getCountAssertionArrayOutOfBound() { return m_countAssertionArrayOutOfBound; }
int Insertion::getCountAssertionIntegerOverflow() { return m_countAssertionIntegerOverflow; }
