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

///////////////// FOR LRU ///////////////////////
typedef struct node { //LRU 캐시를 위한 노드
    int page; // page entry를 저장
    struct node *prev;
    struct node *next;
} node;

node *cache = NULL; // cache를 저장할 연결리스트 선언

void init_cache() {cache = NULL;} // cache를 초기화

node *new_node(int data) { // 새로운 노드를 생성하는 함수
    node *n = (node *)malloc(sizeof(node)); // 노드를 동적할당
    n->page = data;
    n->next = NULL;
    n->prev = NULL;
    return (n);
}

void delete_cache(node *del) { // 노드를 삭제하는 함수
    if (cache == NULL || del == NULL)
        return;
    if (cache == del) // 삭제할 함수가 head
        cache = del->next;
    if (del->next != NULL) // next가 null이 아닐 떄
        del->next->prev = del->prev;
    if (del->prev != NULL) // prev가 null이 아닐 때
        del->prev->next = del->next;
    free(del); // 삭제할 노드를 free
}

void insert_cache(int data) { // 최신의 data를 head에 넣는 함수
    node *ptr = cache; // head를 pointing하는 포인터
    node *n = new_node(data); // 삽입할 새로운 노드 생성
    while (ptr != NULL) {
        if (ptr->page == data) { // 일치하는 데이터(page entry)가 존재한다면
            delete_cache(ptr); // 이미 존재하는 노드를 최신으로 갱신하기 위한 삭제
            break;
        }
        ptr = ptr->next;
    }
    n->prev = NULL;
    n->next = cache; // 새로운 노드를 연결리스트의 제일 앞에 삽입

    if (cache != NULL)
        cache->prev = n;
    cache = n; // 헤드노드를 새로운 노드로 변경
}

////////////////////////////////////////////////////

//TODO-2. 입력된 페이지 오프셋을 통해 페이지 크기 및 페이지 개수를 계산

int powof(int a, int b){
	if (b == 0)
		return (1);
	return (a * powof(a, b-1));
}

void calculatePageInfo(int page_bits) {
    page_size = powof(2,page_bits); // offset개수의 bit로 나타낼 수 있는 경우의 수 만큼 page_size를 설정
    num_pages = powof(2, ADDR_SIZE - page_bits); // page의 개수 = (Logical address의 전체 경우의 수) / (page size)
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
        while (1) {
            victimPage = rand() % num_pages; // random으로 victimPage를 설정함
            if (pageTable[victimPage] != -1) break; // 유효한 페이지면 탈출
        }        
        break;
    case 'a':
    case 'A': // LRU 방식의 Policy 구현
        for(node *ptr = cache; ptr !=NULL; ptr = ptr->next) { // cache에서 제일 마지막 노드(가장 오래전에 사용한 노드)를 찾는 반복문
            if (ptr->next == NULL){
                victimPage = ptr->page; // 찾았다면 해당 pageentry를 victimPage로 선정
                delete_cache(ptr); // 해당 page를 LRU cache에서 삭제
            }
        }
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
    printf("\n(Interrupt) 현재까지 발생한 페이지폴트의 수: %d\n", pageFaults);

    //모든 작업이 완료되었을 시, 시그널 발생 시 동작: 
    if (processDone == 1) { // 전체 process가 종료되었다면
        contFlag = 1; //contFlag를 1로 변화시켜 main함수에서의 while문의 동작을 멈춤
        printf("2018440017 / 김민석(Minseok-Kim)\n\n"); // 학번/이름을 출력
    }
    signal(SIGINT, myHandler); 
}

int main(int argc, char* argv[]) {
    //TODO-1. SIGINT 시그널 발생시 핸들러 myHandler 구현 및 등록(install)
    signal(SIGINT, myHandler); // SIGINT(ctrl+c) 가 입력될 경우 myHandler함수 호출.
    
    srand(time(NULL));
    if (argc <= 2) {       
        printf("please input the parameter! ex)./test 13 d\n");
        printf("1st parameter: page offset in bits\n2nd parameter: replacement policy\n");
        return -1;
    }
    init_cache(); // LRU cache를 초기화함

    int page_bits = atoi(argv[1]);  //입력받은 페이지 오프셋(offset) 크기
    char policy = argv[2][0];       //입력받은 페이지 교체 정책

    //TODO-2. 입력정보를 바탕으로 페이지 크기 및 페이지 개수 계산 
    calculatePageInfo(page_bits);   

    printf("입력된 페이지 별 크기: %dBytes\n프로세스의 페이지 개수: %d개\n페이지 교체 알고리즘: %c\n",
        page_size, num_pages, policy);
    
  
    pageTable = (int*)malloc(num_pages * sizeof(int)); // page의 개수만큼 pagetable을 생성

    for (int i = 0; i < num_pages; i++) 
        pageTable[i] = -1;    

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
        sscanf(line, "%d", &address);

        // 가상 주소에서 페이지 번호(pageNumber)를 얻음
        int pageNumber = getPageNumber(address);

        // pageTable 함수는 페이지 폴트 시 -1 값을 반환함
        int frameNumber = pageTable[pageNumber];
        if (frameNumber == -1) { //page fault
            handlePageFault(pageNumber, policy); //페이지 폴트 핸들러            
            frameNumber = pageTable[pageNumber];
        }
        insert_cache(pageNumber); // cache정보를 업데이트. (이미 존재하는 page라면 최신으로 갱신, 새로운 page라면 삽입->victim page는 pagefault handler에서 이미 삭제)
        //해당 물리 프레임을 접근하고 접근 횟수를 셈
        physicalMemory[frameNumber]++;

        lineNumber++;
        // usleep(1000); //매 페이지 접근 처리 후 0.001초간 멈춤
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
