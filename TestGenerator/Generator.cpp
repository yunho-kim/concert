#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
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
//#include "PPContext.h"

using namespace clang;
using namespace std;

typedef map<string, pair<const Type *, string> > GlobalVarMap;
typedef map<string, QualType> GlobalVarQualTypeMap;
GlobalVarMap TheGlobalVarMap;
GlobalVarQualTypeMap TheGlobalVarQualTypeMap;

enum Option{
    ALL_WR = 0,
    ALL_BR,
    ALL_SS,
};
enum Option Opt = ALL_WR;
bool DoRewrite = false;
int max_depth = 2;
enum FuncType{
    WR = 2,
    BR = 1,
    SS = 0,
};
map<string, enum FuncType> FunctionTypeMap;

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor>
{
public:
    MyASTVisitor(Rewriter &R)
        : TheRewriter(R), TargetFuncName(""), FunctionDeclMap(), 
        SymbolicStubMap(), CurrentFunctionDeclPtr(NULL), HasTargetFunction(false)
    {}
    MyASTVisitor(Rewriter &R, const string &S)
        : TheRewriter(R), TargetFuncName(S), FunctionDeclMap(), 
        SymbolicStubMap(), CurrentFunctionDeclPtr(NULL), HasTargetFunction(false)
    {}
    bool VisitStmt(Stmt *s) {
        // Only care about DeclRefExpr statement that refers to
        // a declared variable, function, enum, etc
        if (isa<DeclRefExpr>(s)){

            DeclRefExpr *DeclRefExprPtr = dyn_cast<DeclRefExpr>(s);
            assert(DeclRefExprPtr != NULL);
    
            NamedDecl *NamedDeclPtr = DeclRefExprPtr->getFoundDecl();
    
            if (!isa<VarDecl>(NamedDeclPtr)){
                return true;
            }
            // Only care about VarDecl type expression, which means
            // the DeclRefExpr is a variable
            VarDecl *VarDeclPtr = dyn_cast<VarDecl>(NamedDeclPtr);
            assert(VarDeclPtr != NULL);
            DeclContext *VarDeclContextPtr = VarDeclPtr->getDeclContext();
            if (VarDeclContextPtr->isFileContext()){
                string VarName = VarDeclPtr->getNameAsString();
                const Type *TypePtr = GetUnqualifiedCanonicalTypePtr(VarDeclPtr->getType());
                const string TypeStr = GetUnqualifiedCanonicalTypeStr(VarDeclPtr->getType());
                if (!VarDeclPtr->getType().isConstQualified() &&
                        TheGlobalVarMap.find(VarName) == TheGlobalVarMap.end()){
                    llvm::errs() << "NamedDecl: " << VarName << "\n";
                    llvm::errs() << "Type: " << TypeStr << "\n";
                    TheGlobalVarMap[VarName] = make_pair<const Type *, string>(TypePtr, TypeStr);
                    TheGlobalVarQualTypeMap[VarName] = VarDeclPtr->getType();
                }
            }
        }
        if (isa<CallExpr>(s)){
            if (Opt == ALL_SS){
                CallExpr * CallExprPtr = dyn_cast<CallExpr>(s);
                FunctionDecl *FunctionDeclPtr = CallExprPtr->getDirectCallee();
                if (FunctionDeclPtr == NULL){
                    return true;
                }
                SourceLocation ST = CallExprPtr->getLocStart();
                string FunctionName = FunctionDeclPtr->getNameInfo().getName().getAsString();
                // Ignore GCC builtin functions
                if (strncmp("__builtin", FunctionName.c_str(), strlen("__builtin")) == 0){
                    return true;
                }
                if (FunctionTypeMap[FunctionName] == WR || 
                        // Skip blackbox real functions
                        strncmp("__BR__", FunctionName.c_str(), 6) == 0 ||
                        // Skip CTC coverage tool probe functions
                        strncmp("ctc_", FunctionName.c_str(), 4) == 0 || 
                        // Skip gcc builtin functions
                        strncmp("__builtin", FunctionName.c_str(), 9) == 0){
                    // Do Nothing
                }else if (FunctionTypeMap[FunctionName] == BR){
                    //TheRewriter.InsertText(ST, "__BR__", true, true);
                }else if (FunctionTypeMap[FunctionName] == SS){
                    string SymbolicStubName = "__sym_" + FunctionName;
                    TheRewriter.InsertText(ST, "__sym_", true, true);
                    if (CurrentFunctionDeclPtr != NULL && 
                            SymbolicStubMap.find(SymbolicStubName) == SymbolicStubMap.end()){
                        stringstream ss;
                        ST = CurrentFunctionDeclPtr->getSourceRange().getBegin();
                        const Type *ReturnTypePtr = GetUnqualifiedCanonicalTypePtr(FunctionDeclPtr->getResultType());
                        string ReturnTypeStr = GetUnqualifiedCanonicalTypeStr(FunctionDeclPtr->getResultType());
                        // Treat a function pointer as a void *
                        if (ReturnTypePtr->isFunctionPointerType()){
                            ReturnTypeStr = "void *";
                        }
                        ss << "static " << ReturnTypeStr << " " << SymbolicStubName << "(";
                        if (FunctionDeclPtr->param_size() == 0){
                            ss << "void";
                        }else{
                            for (int i=0; i<FunctionDeclPtr->param_size(); i++){
                                const ParmVarDecl *ParamVarDeclPtr = FunctionDeclPtr->getParamDecl(i);
                                // Intentionally do not perform canonicalization for handling va_list
                                string ParamTypeStr = ParamVarDeclPtr->getOriginalType().getUnqualifiedType().getAsString();
                                bool isArray = ParamVarDeclPtr->getOriginalType().getTypePtr()->isArrayType();
                                if (isArray && strstr(ParamTypeStr.c_str(), "va_list") == NULL){
                                    llvm::errs() << "ParamTypeStr: " << ParamTypeStr << "\n";
                                    const ArrayType *ArrayTypePtr = dyn_cast<ArrayType>(ParamVarDeclPtr->getOriginalType().getTypePtrOrNull());
                                    ParamTypeStr = ArrayTypePtr->getElementType().getUnqualifiedType().getAsString();
                                }
                                if (ParamVarDeclPtr->getOriginalType().getTypePtr()->isFunctionPointerType()){
                                    ParamTypeStr = "void *";
                                }
                                if (i != 0){
                                    ss << ", ";
                                }
                                ss << ParamTypeStr << " param" << i;
                                if (isArray && strstr(ParamTypeStr.c_str(), "va_list") == NULL){
                                    ss << "[]";
                                }
                           
                            }
                            ss << ", ...";
                        }
                        ss << ");\n";
                        TheRewriter.InsertText(ST, ss.str(), true, true);
                        SymbolicStubMap[SymbolicStubName] = FunctionDeclPtr;
                    }
                }
            }
        }

        return true;
    }
    string SymbolicParamInputSetupStmt(const ParmVarDecl *Param, const string &VarName) {
        assert(Param != NULL);
        const string ParamTypeStr = GetUnqualifiedCanonicalTypeStr(Param->getOriginalType());
        const Type *TypePtr = GetUnqualifiedCanonicalTypePtr(Param->getOriginalType());
        return GetSymbolicInputSetupStmt(TypePtr, ParamTypeStr, VarName, true);
    }
    string GetSymbolicInputSetupStmt(const Type *TypePtr, const string &ParamTypeStr, const string &VarName,const bool isParameter, const int depth=0){
        stringstream out;
        // Check a given variable will be set by C standard library
        // e.g.) stdin, stdout, stderr, ...
        if (VarName == "stdin" || VarName == "stdout" || VarName == "stderr"){
            return "// " + VarName + " will be set by C standard library\n";
        }
        if (ParamTypeStr.find("va_list") != string::npos){
            return "// " + VarName + " is va_list not supported yet\n";
        }
        if (TypePtr->isIntegralOrEnumerationType()) {
            // The parameter is an integral type or enumeration type
            //const BuiltinType *BuiltinTypePtr = dyn_cast<BuiltinType>(TypePtr);
            string CrestSymInputFuncName;
            if (isa<BuiltinType>(TypePtr)) {
                const BuiltinType *BuiltinTypePtr = dyn_cast<BuiltinType>(TypePtr);
                string CrestFuncName = "__Crest";
                switch(BuiltinTypePtr->getKind()) {
                case BuiltinType::Char_S:
                case BuiltinType::SChar:
                    CrestFuncName += "Char";
                    break;
                case BuiltinType::Char_U:
                case BuiltinType::UChar:
                    CrestFuncName += "UChar";
                    break;
                case BuiltinType::Short:
                    CrestFuncName += "Short";
                    break;
                case BuiltinType::UShort:
                    CrestFuncName += "UShort";
                    break;
                case BuiltinType::Int:
                    CrestFuncName += "Int";
                    break;
                case BuiltinType::UInt:
                    CrestFuncName += "UInt";
                    break;
                case BuiltinType::Long:
                    CrestFuncName += "Long";
                    break;
                case BuiltinType::ULong:
                    CrestFuncName += "ULong";
                    break;
                case BuiltinType::LongLong:
                    CrestFuncName += "LongLong";
                    break;
                case BuiltinType::ULongLong:
                    CrestFuncName += "ULongLong";
                    break;
                default:
                    out << "// Type: " << ParamTypeStr << " is handled as int\n";
                    CrestFuncName += "Int";
                }
                out << CrestFuncName << "(&" << VarName << ");\n";
            } else if (TypePtr->isEnumeralType()) {
                out << "__CrestInt(&" << VarName << ");\n";

            }
            else {
                // How to handle this type?
                out << "// Type: " << ParamTypeStr << " of " << VarName << " is not yet supported\n";
            }

        } else if (TypePtr->isFloatingType()) {
            // The parameter is a floating type.
            // Wl give a default value to the floating type parameters
            out << "// Floting Type: " << ParamTypeStr << " of " << VarName << " is not supported\n";
            out << VarName << " = 0;\n";
        } else if ((TypePtr->isArrayType() || (TypePtr->isPointerType() && !(TypePtr->isFunctionPointerType())))) {
            // TODO: Should handle pointer type;
            
            if (depth > max_depth && TypePtr->isPointerType()){
                out << "// Pointer Type: " << ParamTypeStr << " of " << VarName << " is set as NULL\n";
                out << VarName << " = 0;\n";
            }else{
                QualType PointeeType;
                if (TypePtr->isPointerType()){
                    const PointerType *PointerTypePtr = dyn_cast<PointerType>(TypePtr);
                    assert(PointerTypePtr != NULL);
                    PointeeType = PointerTypePtr->getPointeeType();
                }else if (TypePtr->isArrayType()){
                    const ArrayType *ArrayTypePtr = dyn_cast<ArrayType>(TypePtr);
                    assert(ArrayTypePtr != NULL);
                    PointeeType = ArrayTypePtr->getElementType();
                }else{ assert(0); }
                const string PointeeTypeStr = GetUnqualifiedCanonicalTypeStr(PointeeType);
                const Type *PointeeTypePtr = GetUnqualifiedCanonicalTypePtr(PointeeType);
                assert(PointeeTypePtr != NULL);
                static int flagCount = 0;
                if (!PointeeType.isConstQualified()){
                    // Length of array
                    const ConstantArrayType *ConstantArrayTypePtr = dyn_cast<ConstantArrayType>(TypePtr);
                    int len = 1;
                    if (ConstantArrayTypePtr != NULL){
                        len = ConstantArrayTypePtr->getSize().getZExtValue();
                        if (len > 10) len = 10;
                    }
                    else if (PointeeTypePtr->isIntegralOrEnumerationType()){
                        len = 10;
                    }
                    if (!PointeeTypePtr->isIncompleteType()){
                        if (TypePtr->isPointerType() || (TypePtr->isArrayType() && isParameter )){
                            //out << "char flag" << flagCount << ";\n";
                            //out << "__CrestChar(&flag" << flagCount << ");\n";
                            //out << "if(!flag" << flagCount << "){\n";
                            out << VarName << " = malloc(" << len << "*sizeof(" << PointeeTypeStr << "));\n";
                            //out << "}else{\n";
                            //out << VarName << " = ((void *)0);";
                            //out << "}\n";
                            //out << "if(!flag" << flagCount << "){\n";
                        }
                        
                        for (int i = 0; i < len; ++i){
                            stringstream VarNameStream;
                            VarNameStream << VarName << "[" << i << "]";
                            out << GetSymbolicInputSetupStmt(PointeeTypePtr, PointeeTypeStr, VarNameStream.str(), false, depth+1);
                        }
                        if (TypePtr->isPointerType() || (TypePtr->isArrayType() && isParameter )){
                            //out << "}\n";
                            flagCount++;
                        }
                    }else{
                        out << "// Pointee type '" << PointeeTypeStr << "' is an incomplete type\n";
                        out << VarName << " = 0;\n";
                    }
                }else{
                    out << "// Pointee type '" << PointeeTypeStr <<"' is a constant\n";
                    if (!PointeeTypePtr->isIncompleteType()){
                        stringstream ss;
                        ss << "__tmp__" << depth;
                        string TempVarName = ss.str();
                        out << "{\n";
                        int len = 1;
                        const ConstantArrayType *ConstantArrayTypePtr = dyn_cast<ConstantArrayType>(TypePtr);
                        if (ConstantArrayTypePtr != NULL){
                            len = ConstantArrayTypePtr->getSize().getZExtValue();
                            if (len > 10) len = 10;
                        }
                        else if (PointeeTypePtr->isIntegralOrEnumerationType()){
                            len = 10;
                        }

                        //out << "char flag" << flagCount << ";\n";
                        //out << "__CrestChar(&flag" << flagCount << ");\n";
                        out << PointeeTypeStr << " *" << TempVarName << ";\n";
                        //out << "if(!flag" << flagCount << "){\n";

                        out << TempVarName << " = malloc(" << len << "*sizeof(" << PointeeTypeStr << "));\n";
                        for (int i=0; i<len; ++i){
                            stringstream VarNameStream;
                            VarNameStream  << TempVarName << "[" << i << "]";
                            out << GetSymbolicInputSetupStmt(PointeeTypePtr, PointeeTypeStr, VarNameStream.str(), false, depth+1);
                        }
                        //out << "}else{\n";
                        //out << TempVarName << " = ((void *)0);\n";
                        //out << "}\n";
                        out << VarName << " = " << TempVarName << ";\n";
                        out << "}\n";
                    }else{
                        out << "// Pointee type '" << PointeeTypeStr << "' is an incomplete type\n";
                        out << VarName << " = 0;\n";
                    }
                }
            }

        } else if (TypePtr->isStructureType()) {
            const RecordType *RecordTypePtr = dyn_cast<RecordType>(TypePtr);
            assert(RecordTypePtr != NULL);
            const RecordDecl *Record = RecordTypePtr->getDecl();

            assert(Record != NULL);
            for (RecordDecl::field_iterator it = Record->field_begin(); it != Record->field_end(); ++it){
                FieldDecl *Field = *it;
                if (!(Field->isBitField())){
                    // A given field is not a bitfield.
                    const Type *FieldTypePtr = GetUnqualifiedCanonicalTypePtr(Field->getType());
                    assert(FieldTypePtr != NULL);
                    const string FieldTypeStr = GetUnqualifiedCanonicalTypeStr(Field->getType());
                    const string FieldNameStr = Field->getNameAsString();
                    out << GetSymbolicInputSetupStmt(FieldTypePtr, FieldTypeStr, VarName+"."+FieldNameStr, false, depth);
                }else{
                    // A given filed is a bit field.
                    // We currently do not set bitfields as symbolic inputs
                    // because we cannot pass an address of a bitfield to
                    // CREST_XXX() functions.
                    // TODO: We will handle bitfields by introducing an intermediate
                    // variable for setting symbolic inputs like the following
                    //     int x;
                    //     CREST_int(x);
                    //     a.bitfield1 = x; // instead of calling CREST_int(&(a.bitfield1));
                    out << "// Bitfield " << VarName+"."+Field->getNameAsString() << " is not currently supported\n";
                    out << VarName+"."+Field->getNameAsString() << " = 0;\n";
                }
            }
        } else if (TypePtr->isFunctionPointerType()){
            // HACK: hard-coding for grep
            // We will address function pointers in a general way soon.
            if (VarName == "compile"){
                out << VarName << " = Ecompile;\n";
            }else if (VarName == "execute"){
                out << VarName << " = EGexecute;\n";
            }else{
                out << "// Function pointer type: " << ParamTypeStr << " of " << VarName << " is not supported\n";
                out << VarName << " = 0;\n";
            }
        }else {
            // Which type can reach this point?
            out << "// Type: " << ParamTypeStr << " of " << VarName << " is not supported\n";
        }
        return out.str();
    }

    bool VisitFunctionDecl(FunctionDecl *f){
        CurrentFunctionDeclPtr = f;
        string FuncName = f->getNameInfo().getName().getAsString();
        // Skip CTC coverage coverage tool probe functions
        if (strncmp(FuncName.c_str(), "ctc_", 4) == 0) return false;

        if (f->hasBody()){
            if (!f->isInlineSpecified()){
                llvm::errs() << "Defined Function: " << FuncName << "\n";
                FunctionDeclMap[FuncName] = f;
            }
            //FunctionDeclMap[FuncName] = f;
            if (FuncName == "main"){
               SourceLocation ST = f->getSourceRange().getBegin().getLocWithOffset(5);
               stringstream SSAfter;
               SSAfter << "_";
               TheRewriter.InsertText(ST, SSAfter.str(), true, true);
            }
        }
        return true;
    }
    void GenerateTestDriver(){
        if (TargetFuncName != ""){
            if (FunctionDeclMap.find(TargetFuncName) != FunctionDeclMap.end()){
                SourceLocation ST = FunctionDeclMap[TargetFuncName]->getSourceRange().getBegin();
                HasTargetFunction = true;
                DoRewrite = true;

                GenerateTestDriverForEachFunction(FunctionDeclMap[TargetFuncName], ST);
            }else{
                //llvm::errs() << "Cannot find a target function: " << TargetFuncName << "\n";
                //exit(1);
            }
        }else{
            
            for (FunctionDeclMapType::iterator it = FunctionDeclMap.begin();
                    it != FunctionDeclMap.end(); ++it){
                SourceLocation ST = it->second->getSourceRange().getBegin();
                GenerateTestDriverForEachFunction(it->second, ST);
            }
            DoRewrite = true;
            
        }
    }
    void GenerateTestDriver(SourceLocation ST){
        if (TargetFuncName != ""){
            if (FunctionDeclMap.find(TargetFuncName) != FunctionDeclMap.end()){
                HasTargetFunction = true;
                DoRewrite = true;
                GenerateTestDriverForEachFunction(FunctionDeclMap[TargetFuncName], ST);
            }else{
                //llvm::errs() << "Cannot find a target function: " << TargetFuncName << "\n";
                //exit(1);
            }
        }else{

            for (FunctionDeclMapType::iterator it = FunctionDeclMap.begin();
                    it != FunctionDeclMap.end(); ++it){
                GenerateTestDriverForEachFunction(it->second, ST);
            }
            DoRewrite = true;

        }
    }

    void GenerateInitializer(SourceLocation ST, const char *filename){
        //ST = FunctionDeclMap.begin()->second->getSourceRange().getBegin();
        size_t len = strlen(filename);
        char *funcname = new char[len+15];
        strcpy(funcname, "init_");
        strcat(funcname, filename);
        len = strlen(funcname);
        for(size_t i=0; i<len; i++){
            char ch = funcname[i];
            if (isalnum(ch) == false && ch != '_'){
                funcname[i] = '_';
            }
        }
        stringstream SSAfter;
        SSAfter << "\nvoid " << funcname << "(){\n";

        // Setup symbolic inputs for global variables
        for (GlobalVarMap::iterator it = TheGlobalVarMap.begin(); it != TheGlobalVarMap.end(); ++it){
            string VarName = it->first;
            const Type* TypePtr = it->second.first;
            string TypeStr = it->second.second;
            QualType QT = TheGlobalVarQualTypeMap[VarName];
            // Skip CTC coverage tool variables 
            if (strncmp(VarName.c_str(), "ctc_", 4) == 0) continue;
            if (!QT.isConstQualified()){
                SSAfter << GetSymbolicInputSetupStmt(TypePtr, TypeStr, VarName, false);
            }
        }
        SSAfter << "}\n";
        if (Opt == ALL_SS){
            for (SymbolicStubMapType::iterator it = SymbolicStubMap.begin(); 
                    it != SymbolicStubMap.end(); ++it){
                string SymbolicStubName = it->first;
                FunctionDecl *FunctionDeclPtr = it->second;
                const Type *ReturnTypePtr = FunctionDeclPtr->getResultType().getUnqualifiedType().getCanonicalType().getTypePtrOrNull();
                string ReturnTypeStr = FunctionDeclPtr->getResultType().getUnqualifiedType().getCanonicalType().getAsString();
                if (ReturnTypePtr->isFunctionPointerType()){
                    ReturnTypeStr = "void *";
                }

                SSAfter << "static " << ReturnTypeStr << " " << SymbolicStubName << "(";
                if (FunctionDeclPtr->param_size() == 0){
                    SSAfter << "void";
                }else{
                    for (int i=0; i<FunctionDeclPtr->param_size(); i++){
                        const ParmVarDecl *ParamVarDeclPtr = FunctionDeclPtr->getParamDecl(i);
                        //string ParamTypeStr = ParamVarDeclPtr->getOriginalType().getUnqualifiedType().getCanonicalType().getAsString();
                        string ParamTypeStr = ParamVarDeclPtr->getOriginalType().getUnqualifiedType().getAsString();
                        bool isArray = ParamVarDeclPtr->getOriginalType().getTypePtr()->isArrayType();
                        if (ParamVarDeclPtr->getOriginalType().getTypePtr()->isFunctionPointerType()){
                            ParamTypeStr = "void *";
                        }
                        if (isArray && strstr(ParamTypeStr.c_str(), "va_list") == NULL){
                            llvm::errs() << "ParamTypeStr: " << ParamTypeStr << "\n";
                            const ArrayType *ArrayTypePtr = dyn_cast<ArrayType>(ParamVarDeclPtr->getOriginalType().getTypePtrOrNull());
                            ParamTypeStr = ArrayTypePtr->getElementType().getUnqualifiedType().getAsString();
                        }

                        if (i != 0){
                            SSAfter << ", ";
                        }
                        SSAfter << ParamTypeStr << " param" << i;
                        if (isArray && strstr(ParamTypeStr.c_str(), "va_list") == NULL){
                            SSAfter << "[]";
                        }
                    }
                    SSAfter << ", ...";
                }
                SSAfter << "){\n";
                if (!FunctionDeclPtr->isNoReturn() && ReturnTypeStr != "void"){
                    SSAfter << "  " << ReturnTypeStr << " ret;\n";
                    SSAfter << GetSymbolicInputSetupStmt(ReturnTypePtr, ReturnTypeStr, "ret" , false/*, 100*/);
                    SSAfter << "  return ret;\n";
                }
                SSAfter << "}\n";
            }
        }
        TheRewriter.InsertText(ST, SSAfter.str(), true, true);
        if (Opt != ALL_SS || HasTargetFunction){
            FILE *fp = fopen("initializer.dat", "a");
            fprintf(fp, "%s\n", funcname);
            fclose(fp);
        }

      
    }
    bool GenerateTestDriverForEachFunction(FunctionDecl *f, SourceLocation ST) {
        // Only function definitions (with bodies), not declarations.
        if (f->hasBody() &&
                (TargetFuncName == "" ||
                 f->getNameInfo().getName().getAsString() == TargetFuncName)) {
            Stmt *FuncBody = f->getBody();

            // Type name as string
            QualType QT = f->getResultType();
            string TypeStr = QT.getAsString();

            // Function name
            DeclarationName DeclName = f->getNameInfo().getName();
            string FuncName = DeclName.getAsString();

            // Test driver function name
            string DriverFuncName = FuncName + "_driver";

            stringstream SSAfter;

            SSAfter << "\n\n// Test driver for function " << FuncName << " returning "
                     << TypeStr << "\n";
            // Add function prototype
            SSAfter << TypeStr << " " << DriverFuncName << "()" << "\n";

            // Start of a body of the test driver
            SSAfter << "{" << "\n";

            // Declare local variables passed to a target function as parameters
            for (FunctionDecl::param_const_iterator it = f->param_begin(); it != f->param_end(); ++it) {
                const ParmVarDecl *param = *it;
                string ParamTypeStr = GetUnqualifiedCanonicalTypeStr(param->getOriginalType());
                const Type *TypePtr = GetUnqualifiedCanonicalTypePtr(param->getOriginalType());
                if (strstr(ParamTypeStr.c_str(), "const") != NULL){
                    const char *p = strstr(ParamTypeStr.c_str(), "const");
                    int offset = p - ParamTypeStr.c_str();
                    ParamTypeStr[offset+0] = ParamTypeStr[offset+1] = ParamTypeStr[offset+2] =
                    ParamTypeStr[offset+3] = ParamTypeStr[offset+4] = ' ';

                }
                if (TypePtr->isFunctionPointerType()){
                    ParamTypeStr = "void *";
                } else if (TypePtr->isArrayType()) {
                    const ArrayType * arrayType = dyn_cast<ArrayType>(TypePtr);
                    assert(arrayType != NULL);
                    ParamTypeStr = GetUnqualifiedCanonicalTypeStr(arrayType->getElementType()) + "*";    
                }
                assert(TypePtr != NULL);

                SSAfter << ParamTypeStr << " param" << it - f->param_begin() + 1 << ";\n";
            }

            // Setup symbolic inputs for parameters
            for (FunctionDecl::param_const_iterator it = f->param_begin(); it != f->param_end(); ++it) {
                const ParmVarDecl *param = *it;
                int ParamNum = it - f->param_begin() + 1;
                stringstream ParamNameStream;

                ParamNameStream << "param" << ParamNum;
                SSAfter << SymbolicParamInputSetupStmt(param, ParamNameStream.str());

            }
            // Setup symbolic inputs for global variables
            for (GlobalVarMap::iterator it = TheGlobalVarMap.begin(); it != TheGlobalVarMap.end(); ++it){
                string VarName = it->first;
                const Type* TypePtr = it->second.first;
                string TypeStr = it->second.second;
                // Skip global variables for CTC coverage tool
                // Name of the variables starts with 'ctc_'
                if (strncmp(VarName.c_str(), "ctc_", 4) != 0){
                    SSAfter << GetSymbolicInputSetupStmt(TypePtr, TypeStr, VarName, false);
                }
            }

            // Call the target function
            if (f->param_size() == 0) {
                SSAfter << "return " << FuncName << "();\n";
            } else {
                SSAfter << "return " << FuncName << "(param1";
                for (unsigned i = 1; i < f->param_size(); ++i) {
                    SSAfter << ",param" << i+1;
                }
                SSAfter << ");\n";
            }


            // End of a body of the test driver
            SSAfter << "}" << "\n";

            // Add main()
            if (TargetFuncName != ""){
                SSAfter << "int test_main(){\n";
                SSAfter << DriverFuncName << "();\n";
                SSAfter << " return 0;}\n";
            }

            TheRewriter.InsertText(ST, SSAfter.str(), true, true);
        }

        return true;
    }

private:
    const Type *GetUnqualifiedCanonicalTypePtr(QualType QT){
        return QT.getUnqualifiedType().getCanonicalType().getTypePtrOrNull(); 
    }
    string GetUnqualifiedCanonicalTypeStr(QualType QT){
        return QT.getUnqualifiedType().getCanonicalType().getAsString();
    }

    Rewriter &TheRewriter;
    const string TargetFuncName;
    typedef map<string, FunctionDecl *> FunctionDeclMapType;
    typedef map<string, FunctionDecl *> SymbolicStubMapType;
    FunctionDeclMapType FunctionDeclMap;
    SymbolicStubMapType SymbolicStubMap;
    const FunctionDecl *CurrentFunctionDeclPtr;
    bool HasTargetFunction;

};
// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer
{
public:
    MyASTConsumer(Rewriter &R)
        : Visitor(R)
    {}
    MyASTConsumer(Rewriter &R, const string &S)
        : Visitor(R, S)
    {}
    // Override the method that gets called for each parsed top-level
    // declaration.
    virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
        for (DeclGroupRef::iterator b = DR.begin(), e = DR.end();
                b != e; ++b){
            // Traverse the declaration using our AST visitor.
            Visitor.TraverseDecl(*b);
        }
        return true;
    }
    void GenerateTestDriver(){
        Visitor.GenerateTestDriver();
    }
    void GenerateTestDriver(SourceLocation ST){
        Visitor.GenerateTestDriver(ST);
    }
    void GenerateInitializer(SourceLocation ST, const char *filename){
        Visitor.GenerateInitializer(ST, filename);
    }

private:
    MyASTVisitor Visitor;
};


int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 6) {
        llvm::errs() << "Usage: generator <filename> [funcname] [{ALL_WR,ALL_SS}]\n";
        return 1;
    }
    if (argc >= 4 && strcmp(argv[3], "ALL_SS") == 0){
        Opt = ALL_SS;
    }else if (argc == 3 && strcmp(argv[2], "ALL_SS") == 0){
        Opt = ALL_SS;
    }else{
        Opt = ALL_WR;
    }
    char *DepthStr = getenv("DEPTH");
    if (DepthStr){
        max_depth = atoi(DepthStr);
    }
    char *ssfuncs = getenv("SSFUNCS");
    if (ssfuncs){
        ifstream in(ssfuncs);
        string fname;
        if (in != NULL){
            while (in >> fname){
                FunctionTypeMap[fname] = SS;
                llvm::errs() << "SSFUNC: " << fname << "\n";
            }
        }
    }

    char *wrfuncs = getenv("WRFUNCS");
    if (wrfuncs){
        ifstream in(wrfuncs);
        string fname;
        if (in != NULL){
            while (in >> fname){
                FunctionTypeMap[fname] = WR;
            }
        }
    }
    char *brfuncs = getenv("BRFUNCS");
    if (brfuncs){
        llvm::errs() << "Handle BR functions" << "\n";
        ifstream in(brfuncs);
        string fname;
        if (in != NULL){
            while (in >> fname){
                FunctionTypeMap[fname] = BR;
                llvm::errs() << "BRFUNC: " << fname << "\n";
            }
        }
    }

    string fname;


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

    Rewriter OriginalFileWriter;
    OriginalFileWriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());
    // Set the main file handled by the source manager to the input file.
    const FileEntry *FileIn = FileMgr.getFile(argv[1]);
    FileID FID = SourceMgr.createMainFileID(FileIn);
    TheCompInst.getDiagnosticClient().BeginSourceFile(
        TheCompInst.getLangOpts(),
        &TheCompInst.getPreprocessor());

    // Set preprocessor
    //Preprocessor pp(TheCompInst.getDiagnostics(), TheCompInst.getLangOpts(), 
    //                TI, TheCompInst.getSourceManager(), TheCompInst.getFileManager());
    MyASTConsumer *TheConsumer;
    if (argc >= 3 && strcmp(argv[2], "ALL_SS") != 0)
        TheConsumer = new MyASTConsumer(TheRewriter, argv[2]);
    else
        TheConsumer = new MyASTConsumer(TheRewriter);

    // Parse the file to AST, registering our consumer as the AST consumer.
    ParseAST(TheCompInst.getPreprocessor(), TheConsumer,
             TheCompInst.getASTContext());
    //TheConsumer->GenerateTestDriver();
    TheConsumer->GenerateTestDriver(SourceMgr.getLocForEndOfFile(SourceMgr.getMainFileID()).getLocWithOffset(0));

    TheConsumer->GenerateInitializer(SourceMgr.getLocForEndOfFile(SourceMgr.getMainFileID()).getLocWithOffset(0), argv[1]);
    // At this point the rewriter's buffer should be full with the rewritten
    // file contents.
    const RewriteBuffer *RewriteBuf;
    if (Opt != ALL_SS || DoRewrite){
        RewriteBuf = TheRewriter.getRewriteBufferFor(SourceMgr.getMainFileID());
        llvm::outs() << string(RewriteBuf->begin(), RewriteBuf->end()); 
    }else{
        char cmd[1000]={0,};
        snprintf(cmd, sizeof(cmd), "cat %s", argv[1]);
        system(cmd);
    }
    return 0;
}
