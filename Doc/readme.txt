0. Clang/LLVM 3.3 설치 방법

Suse Linux 11.1 의 낮은 커널 버전 문제와 GCC 4.3.2 컴파일러의
오류로 Clang/LLVM 3.3 버전을 사용해야 유닛 테스트 생성 도구를 
사용할 수 있다. 또한, GCC 4.3.2 컴파일러의 버그로 인해 미리 
문제가 발생할 수 있는 소스코드와 설정을 수정한 버전을 
다음 링크에서 다운받는다.

http://swtv.kaist.ac.kr/~yhkim/llvm-3.3.src.tar.gz

다운로드 완료 후 다음 명령어를 순차대로 수행한다.

$ tar -zxf llvm-3.3.src.tar.gz
$ mkdir llvm-3.3.build
$ cd llvm-3.3.build
$ ../llvm-3.3.src/configure --prefix=${HOME}/llvm --with-optimize-option="-O0"
$ make -jN && make install # N은 사용 가능한 최대 core 수
$ ${HOME}/llvm/bin/clang # clang 이 정상적으로 실행 가능한지 확인

1. CONCERT 다운로드 및 설치 방법

a) git clone 사용
git 을 사용 가능할 경우 다음 커맨드로 github에서 저장소를 복제한다.
Suse Linux 에서 제공하는 git의 버전이 낮아 github의 저장소 복제가
불가능할 수 있으며 해당 경우는 b) 방법 사용

$ git clone https://github.com/yunho-kim/concert.git

b) github 에서 다운로드

$ wget https://github.com/yunho-kim/concert/archive/master.zip
$ unzip master.zip
$ mv concert-master concert

다운로드 및 압축해제가 완료되면 다음 커맨드를 실행

$ cd concert/TestGenerator
$ make
$ cd ../

2. CONCERT 환경 변수 설정

Conf/conf.sh 파일을 수정하여 CONCERT 및 CREST 설치 디렉토리를
설정하고 타겟 소스 코드의 파일 목록과 테스트 대상 함수 목록을 설정한다.
자세한 사항은 conf.sh 파일과 conf.sh.example 파일을 참조한다. 

설정 완료 후 다음 커맨드를 실행하여 설정 파일의 환경 변수를 등록한다.

$ source Conf/conf.sh

3. CONCERT 실행 방법

먼저 테스트를 생성할 디렉토리로 이동한다. 본 설명에서는 
${HOME}/work 디렉토리를 생성하고 사용한다.

$ mkdir ${HOME}/work && cd ${HOME}/work

그 후 concert/Scripts의 스크립트를 다음 순서대로 실행한다.
$ concert/Scripts/gen_test.sh
$ concert/Scripts/compile.sh
$ concert/Scripts/link.sh

스크립트 실행이 성공적으로 완료되면 test_FUNC 디렉토리와 그 안의 실행파일이 
생성된다.

4. CREST 실행 방법
CREST가 ${CRESTDIR} 디렉토리에 설치되어 있다고 가정한다.

$ cd test_FUNC
$ ${CRESTDIR}/bin/run_crest "./test_FUNC" 100 -dfs # 100개의 TC를 DFS 탐색으로 생성

5. 전처리 된 소스 코드 파일을 얻는 방법
CONCERT는 전처리가 끝난 (#include, #define 등이 모두 적용되어 사라진 상태) 소스 코드
파일을 입력으로 받기 때문에 CONCERT를 적용하기 전에 타겟 프로그램의 전처리 된 소스 
코드 파일을 얻어야 한다. gcc 에서는 --save-temps 옵션을 사용해서 전처리 된 소스 코드 
파일을 얻을 수 있다. 다음 2개의 옵션 중 a를 먼저 시도하고 a가 어려울 경우 b의 
방법을 사용한다.

a) CFLAGS 수정
타겟 빌드 스크립트(예: Makefile)를 수정하여 CFLAGS 에 --save-temps 옵션 추가

b) alias 설정
alias gcc='gcc --save-temps' 를 설정하여 gcc 커맨드가 항상 gcc --save-temps 를 
사용하도록 지정
