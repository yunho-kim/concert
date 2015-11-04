#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <utility>
#include <getopt.h>

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Lex/HeaderSearch.h"

#include "Insertion.h"

using namespace clang;
using namespace std;

const char* param_inputFile = NULL;
bool param_isStdout = true;
const char* param_outputFile;
bool param_useDivideByZero = false;
bool param_useArrayOutOfBound = false;
bool param_useNullPointerDereference = false;
bool param_useAll = false;

void run();

int main(int argc, char *argv[])
{
	int c;
	int digit_optind = 0;

	while (1) {
	   int this_option_optind = optind ? optind : 1;
	   int option_index = 0;
	   static struct option long_options[] = {
		   {"output",					required_argument,	0,  'o' },
		   {0,         					0,              	0,	0 }
	   };

	   c = getopt_long(argc, argv, "o:0", long_options, &option_index);
	   if (c == -1)
		   break;

	   switch (c) {
		case 'o':
			param_outputFile = optarg;
			param_isStdout = false;
			break;
	   default:
			break;
	   }
	}
	
	if (optind+1 == argc) {
		param_inputFile = argv[optind];
		run();
		return 0;
	}
	else {
		llvm::errs() << "Usage: AssertionInjector [-o <outputFile>] <filename>\n\n";
        return 1;
	}
}


void run() {
	// CompilerInstance will hold the instance of the Clang compiler for us,
    // managing the various objects needed to run the compiler.
    CompilerInstance TheCompInst;
    TheCompInst.createDiagnostics(NULL, false);

    // Initialize target info with the default triple for our platform.
    TargetOptions *TO = new TargetOptions();
    TO->Triple = llvm::sys::getDefaultTargetTriple();
    TargetInfo *TI = TargetInfo::CreateTargetInfo(
                         TheCompInst.getDiagnostics(), TO);
    TheCompInst.setTarget(TI);

    TheCompInst.createFileManager();
    FileManager &FileMgr = TheCompInst.getFileManager();
    TheCompInst.createSourceManager(FileMgr);
    SourceManager &SourceMgr = TheCompInst.getSourceManager();
    TheCompInst.createPreprocessor();
    TheCompInst.createASTContext();

    // A Rewriter helps us manage the code rewriting task.
    Rewriter TheRewriter;
    TheRewriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());

    // Set the main file handled by the source manager to the input file.
    const FileEntry *FileIn = FileMgr.getFile(param_inputFile);
    SourceMgr.createMainFileID(FileIn);
    TheCompInst.getDiagnosticClient().BeginSourceFile(TheCompInst.getLangOpts(),&TheCompInst.getPreprocessor());
    // Create an AST consumer instance which is going to get called by ParseAST.
	
	Insertion* consumer = new Insertion(&TheRewriter, &TheCompInst.getSourceManager(), &TheCompInst.getASTContext(), &TheCompInst.getLangOpts());	
	Preprocessor &preprocessor = TheCompInst.getPreprocessor();
	//preprocessor.SetSuppressIncludeNotFoundError(true);
	
	/*
	HeaderSearch & headerSearch = preprocessor.getHeaderSearchInfo();
	headerSearch.AddSearchPath( DirectoryLookup( FileMgr.getDirectory("/usr/include/") , clang::SrcMgr::C_System , false ) ,  true );
	headerSearch.AddSearchPath( DirectoryLookup( FileMgr.getDirectory("/usr/include/x86_64-linux-gnu/") , clang::SrcMgr::C_System , false ) ,  true );
	headerSearch.AddSearchPath( DirectoryLookup( FileMgr.getDirectory("/usr/include/linux/") , clang::SrcMgr::C_System , false ) ,  true );
	headerSearch.AddSearchPath( DirectoryLookup( FileMgr.getDirectory("/usr/include/c++/4.7/tr1/") , clang::SrcMgr::C_System , false ) ,  true );
	headerSearch.AddSearchPath( DirectoryLookup( FileMgr.getDirectory("/usr/include/c++/4.7/") , clang::SrcMgr::C_System , false ) ,  true );
	*/
	
	 
	ParseAST(preprocessor, consumer, TheCompInst.getASTContext());
	const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(SourceMgr.getMainFileID());
	
	if( RewriteBuf && TheRewriter.buffer_begin() !=  TheRewriter.buffer_end() ) {
		if(param_isStdout)
			llvm::outs() << string(RewriteBuf->begin(), RewriteBuf->end());	
		else {
			ofstream output(param_outputFile);
			output << string(RewriteBuf->begin(), RewriteBuf->end());
			output.close();
		}
		
	} 
	
	printf("DivideByZero\t%s\t%d\n", param_inputFile, consumer->getCountAssertionDivideByZero() );
	printf("NullPointerDereference\t%s\t%d\n", param_inputFile, consumer->getCountAssertionNullPointerDereference() );
	printf("ArrayOutOfBound\t%s\t%d\n", param_inputFile, consumer->getCountAssertionArrayOutOfBound() );
    printf("IntegerOverflow\t%s\t%d\n", param_inputFile, consumer->getCountAssertionIntegerOverflow() );
	
	delete consumer;
}



