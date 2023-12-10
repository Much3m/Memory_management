#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>

#define ADDR_SIZE 27 //주어진 시스템은 27-bit 주소를 사용함
#define NUM_FRAMES 64 //주어진 시스템은 64개의 물리 프레임이 있음
#define MAX_LINE_SIZE 20000 

int page_size = -1; //한 페이지의 크기(Byte)
int num_pages = -1; //각 프로세스의 페이지 개수

// 페이지 테이블
int* pageTable = NULL;

// 물리적 메모리 페이지(프레임)
int physicalMemory[NUM_FRAMES];

// 페이지 폴트 및 페이지 교체 횟수
unsigned int pageFaults = 0;


// 프로세스의 종료 구분 플래그
int processDone = 0;
// 프로세스를 종료하지 않기 위한 플래그
int contFlag = 0;

//TODO-2. 입력된 페이지 오프셋을 통해 페이지 크기 및 페이지 개수를 계산
void calculatePageInfo(int page_bits) {
    page_size = //FILL ME
    num_pages = //FILL ME
}

// 가상 주소에서 페이지 번호 추출
int getPageNumber(int virtualAddress) {
    return virtualAddress / page_size;
}


//TODO-4. 이 함수는 교체 정책(policy)을 선택 후, 
//알고리즘에 따른 교체될 페이지(victimPage)를 지정.
//교체될 페이지가 존재했던 물리 프레임 번호(frameNumber)를 반환(return). 
int doPageReplacement(char policy) {
    int victimPage = -1; //교체될(evictee) 페이지 번호
    int frameNumber = -1; //페이지 교체를 통해 사용가능한 프레임 번호(return value)

    static int defaultVictim = 0;// 샘플 교체정책에 사용되는 변수
    switch (policy) {
    case 'd': //샘플: 기본(default) 교체 정책
    case 'D': //순차교체: 교체될 페이지 엔트리 번호를 순차적으로 증가시킴 
        while (1) { //유효한(물리프레임에 저장된) 페이지를 순차적으로 찾음
            if (pageTable[defaultVictim] != -1) {                
                break;
            }            
            defaultVictim = (defaultVictim + 1) % num_pages;
        }
        victimPage = defaultVictim;
        break;
    case 'r': 
    case 'R': //TODO-4-1: 교체 페이지를 임의(random)로 선정
        // rand() 함수를 사용.
        // 기타 지역변수,정적변수 필요한 경우 추가하여 사용        
        break;
    case 'a':
    case 'A':
        //(Option)TODO-4-2: 기타 교체 정책 구현해보기
        //FIFO, LRU, 나만의 정책 등, 다른 교체 알고리즘울 조사 후 구현
        //1개 이상의 교체 정책 추가로 구현 가능. 
        //다수의 알고리즘 개발 시, 알고리즘 선택 변수(char policy)는 a,b,c,...순으로 구현 
        break;

    default:
        printf("ERROR: 정의되지 않은 페이지 교체 정책\n");
        exit(1);
        break;
    }      

    frameNumber = pageTable[victimPage]; //교체된 페이지를 통해 사용 가능해진 물리 프레임 번호
    pageTable[victimPage] = -1;  //교체된 페이지는 더 이상 물리 메모리에 있지 않음을 기록  

    return frameNumber;
}

// 페이지 폴트 처리
void handlePageFault(int pageNumber, char policy) {
    int frameNumber = -1; //페이지 폴트 시 사용할 물리 페이지 번호(index)

    // 사용하지 않는 프레임을 순차적으로 할당함
    // (p.s. 실제 시스템은 이런식으로 순차할당하지 않습니다.) 
    static int nextFrameNumber = 0; 
    static int frameFull = -1;      //모든 물리프레임이 사용된 경우 1, 아닌 경우 -1

    //물리 프레임에 여유가 있는 경우
    if (frameFull == -1) { 
        frameNumber = nextFrameNumber++;  
        //모든 물리 프레임이 사용된 경우, 이를 마크함
        if(nextFrameNumber == NUM_FRAMES) 
            frameFull = 1;   
    }
    //모든 물리 프레임이 사용중. 기존 페이지를 교체해야 함
    else { 
        //TODO-4. 페이지 교체 알고리즘을 통해 교체될 페이지가 저장된 물리 프레임 번호를 구함
        frameNumber = doPageReplacement(policy); 
    }

    // 페이지 테이블 업데이트
    pageTable[pageNumber] = frameNumber;

    printf("페이지 폴트 발생: 페이지 %d를 프레임 %d로 로드\n", pageNumber, frameNumber);
    pageFaults++;
}


//TODO-1. 'Ctrl+C' 키보드 인터럽트(SIGINT) 발생 시 처리 루틴
void myHandler() {
    //SIGINT 시그널 발생 시 기본 동작
    printf("\n(Interrupt) 현재까지 발생한 페이지폴트의 수: %d\n", pageFaults);

    //모든 작업이 완료되었을 시, 시그널 발생 시 동작: 
    if (/* FILL ME */) {
        //1. cont_flag 변수를 수정하여 프로세스가 종료될 수 있도록 함. 
        //2. '학번/이름'을 출력함
    }
}

int main(int argc, char* argv[]) {
    //TODO-1. SIGINT 시그널 발생시 핸들러 myHandler 구현 및 등록(install)
    
    srand(time(NULL));
    if (argc <= 2) {       
        printf("please input the parameter! ex)./test 13 d\n");
        printf("1st parameter: page offset in bits\n2nd parameter: replacement policy\n");
        return -1;
    }

    int page_bits = atoi(argv[1]);  //입력받은 페이지 오프셋(offset) 크기
    char policy = argv[2][0];       //입력받은 페이지 교체 정책

    //TODO-2. 입력정보를 바탕으로 페이지 크기 및 페이지 개수 계산 
    calculatePageInfo(page_bits);   

    printf("입력된 페이지 별 크기: %dBytes\n프로세스의 페이지 개수: %d개\n페이지 교체 알고리즘: %c\n",
        page_size, num_pages, policy);
    
  
    // 페이지 테이블 할당 및 초기화
    pageTable = (int*)malloc(/*TODO-3*/);
    // 페이지 테이블 초기화
    for (int i = 0; i < num_pages; i++) 
        pageTable[i] = -1;    
    // 물리 프레임 초기화
    for (int i = 0; i < NUM_FRAMES; i++) 
        physicalMemory[i] = 0;
    

    // 파일 읽기
    const char* filename = "input.txt";
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("파일 열기 오류");
        return EXIT_FAILURE;
    }

    // 파일 내 데이터: 가상 메모리 주소
    // 모든 메모리 주소에 대해서
    int lineNumber = 0;
    while (!feof(file)) {
        char line[MAX_LINE_SIZE];
        fgets(line, MAX_LINE_SIZE, file);

        int address;        
        sscanf(line, "%d", address);

        // 가상 주소에서 페이지 번호(pageNumber)를 얻음
        int pageNumber = getPageNumber(address);

        // pageTable 함수는 페이지 폴트 시 -1 값을 반환함
        int frameNumber = pageTable[pageNumber];
        if (frameNumber == -1) { //page fault
            handlePageFault(pageNumber, policy); //페이지 폴트 핸들러            
            frameNumber = pageTable[pageNumber];
        }
                        
        //해당 물리 프레임을 접근하고 접근 횟수를 셈
        physicalMemory[frameNumber]++;

        lineNumber++;
        usleep(1000); //매 페이지 접근 처리 후 0.001초간 멈춤
        //이 delay는 프로세스 수행 중, signal발생 처리과정을 확인하기 위함이며,
        //구현을 수행하는 도중에는 주석처리하여, 빠르게 결과확인을 하기 바랍니다.
    }

    fclose(file); 
    free(pageTable);

    // 작업 수행 완료. Alarm 시그널을 기다림.
    processDone = 1;
    printf("프로세스가 완료되었습니다. 종료 신호를 기다리는 중...\n");    
    while (contFlag == 0){};


    // 결과 출력
    printf("\n---물리 프레임 별 접근 횟수----\n");
    for (int i = 0; i < NUM_FRAMES; i++) {
        printf("[%03d]frame: %d\n", i, physicalMemory[i]);
    }
    printf("----------\n페이지 폴트 횟수: %d\n", pageFaults);
 
    return 0;
}
