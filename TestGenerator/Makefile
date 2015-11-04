CXX = g++
CFLAGS = -fno-rtti -g -O0 -I/local_home/yhkim/llvm/include -I/local_home/yhkim/build/include  -D_DEBUG -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -fomit-frame-pointer -fvisibility-inlines-hidden -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual

LLVM_SRC_PATH =/local_home/yhkim/llvm
LLVM_BUILD_PATH =/local_home/yhkim/build

LLVM_BIN_PATH = $(LLVM_BUILD_PATH)/Release+Asserts/bin
LLVM_LIBS=core mc all
LLVM_CONFIG_COMMAND = $(LLVM_BIN_PATH)/llvm-config  \
					  --ldflags --libs $(LLVM_LIBS)
CLANG_BUILD_FLAGS = -I$(LLVM_SRC_PATH)/tools/clang/include \
					-I$(LLVM_BUILD_PATH)/tools/clang/include

CLANGLIBS = \
	-lclangFrontendTool -lclangFrontend -lclangDriver \
	-lclangSerialization -lclangCodeGen -lclangParse \
	-lclangSema -lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore \
	-lclangAnalysis -lclangARCMigrate -lclangRewriteCore -lclangRewriteFrontend \
	-lclangEdit -lclangAST -lclangLex -lclangBasic

all: Generator

Generator: Generator.cpp
	$(CXX) Generator.cpp $(CFLAGS) -o Generator \
	$(CLANG_BUILD_FLAGS) $(CLANGLIBS) `$(LLVM_CONFIG_COMMAND)`

clean:
	rm -rf *.o *.ll Generator
