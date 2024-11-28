#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
// #include <stdio.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// bmp header
#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;
    //DIB
    unsigned int   biSize;
    int            biWidth;
    int            biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int   biCompression;
    unsigned int   biSizeImage;
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} BmpHeader;
#pragma pack(pop)

// linked list node
typedef struct Node {
    unsigned char data;
    struct Node* next;
} Node;

// linked list queue
typedef struct {
    Node* front;
    Node* rear;
    // pthread_mutex_t lock; //mutex 추가
} Queue;

// thread parameter
typedef struct {
    Queue* queue;
    unsigned int thread_id;
    unsigned int strBfOffBits;
    unsigned int calNum;
    unsigned int filesize;
    unsigned int open_fd;
} ThreadArgv;

void enqueue(Queue* queue, unsigned char* value) {
    Node* new_node = (Node*)malloc(sizeof(Node));  // 새로운 노드 할당

    new_node->data = *value;
    new_node->next = NULL;

    pthread_mutex_lock(&mutex);  // 큐 수정 시 뮤텍스 잠금

    if (queue->rear == NULL) {
        // 큐가 비어있다면, new_node를 front와 rear로 설정
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        // 큐에 데이터가 있을 때, 오름차순으로 삽입 위치 찾기
        Node* current = queue->front;
        Node* previous = NULL;

        // 새 노드를 삽입할 위치를 찾기 (오름차순)
        while (current != NULL && current->data < new_node->data) {
            previous = current;
            current = current->next;
        }

        if (previous == NULL) {
            // 새 노드가 가장 앞에 와야 하는 경우
            new_node->next = queue->front;
            queue->front = new_node;
        } else if (current == NULL) {
            // 새 노드가 가장 뒤에 와야 하는 경우
            queue->rear->next = new_node;
            queue->rear = new_node;
        } else {
            // 중간에 삽입되는 경우
            previous->next = new_node;
            new_node->next = current;
        }
    }
    // printf("Enqueue: %d\n", *value);

    pthread_mutex_unlock(&mutex);  // 큐 수정 후 뮤텍스 잠금 해제
}


void* thread_cal(void* arg) {
    ThreadArgv* threadArg = (ThreadArgv*)arg;
    unsigned int start_offset = threadArg->strBfOffBits;
    unsigned int calNum = threadArg->calNum;
    // printf("thread %d, offset %d, calNum %d, fileSize %d\n",threadArg->thread_id, start_offset, calNum, threadArg->filesize);
    Queue* queue = threadArg->queue;

    int open_fd = threadArg->open_fd;
    // 데이터 읽기 시작 위치로 파일 오프셋 이동
    lseek(open_fd, start_offset, SEEK_SET);

    unsigned char buffer[8];  // 버퍼 크기
    int fileSize = threadArg->filesize;

    for (int j = 0; j < calNum; j += sizeof(buffer)) {
        ssize_t sizeRead = read(open_fd, buffer, sizeof(buffer));
        if (sizeRead == 0) break;  // 파일 끝에 도달하면 종료

        // printf("Thread %d offset: %lu\n", threadArg->thread_id, start_offset + j);

        for (int i = 0; i < sizeRead; i++) {
            enqueue(queue, &buffer[i]);  // 읽은 데이터를 큐에 삽입
        }
    }

    // printf("num는 %d, 값은 %d\n",threadArg->thread_id, p);

    return NULL;
}


//queue 초기화 함수
void init_queue(Queue* queue) {
    queue->front = NULL;  
    queue->rear = NULL;   
    pthread_mutex_init(&mutex, NULL); 
}

void read_header(BmpHeader* fileHeader, int open_fd) {

    read(open_fd, &fileHeader->bfType, sizeof(fileHeader->bfType));
    read(open_fd, &fileHeader->bfSize, sizeof(fileHeader->bfSize));
    read(open_fd, &fileHeader->bfReserved1, sizeof(fileHeader->bfReserved1));
    read(open_fd, &fileHeader->bfReserved2, sizeof(fileHeader->bfReserved2));
    read(open_fd, &fileHeader->bfOffBits, sizeof(fileHeader->bfOffBits));
    read(open_fd, &fileHeader->biSize, sizeof(fileHeader->biSize));
    read(open_fd, &fileHeader->biWidth, sizeof(fileHeader->biWidth));
    read(open_fd, &fileHeader->biHeight, sizeof(fileHeader->biHeight));
    read(open_fd, &fileHeader->biPlanes, sizeof(fileHeader->biPlanes));
    read(open_fd, &fileHeader->biBitCount, sizeof(fileHeader->biBitCount));
    read(open_fd, &fileHeader->biCompression, sizeof(fileHeader->biCompression));
    read(open_fd, &fileHeader->biSizeImage, sizeof(fileHeader->biSizeImage));
    read(open_fd, &fileHeader->biXPelsPerMeter, sizeof(fileHeader->biXPelsPerMeter));
    read(open_fd, &fileHeader->biYPelsPerMeter, sizeof(fileHeader->biYPelsPerMeter));
    read(open_fd, &fileHeader->biClrUsed, sizeof(fileHeader->biClrUsed));
    read(open_fd, &fileHeader->biClrImportant, sizeof(fileHeader->biClrImportant));

}

void made_output(BmpHeader* fileHeader, char* color_table, off_t fileSize, int color_table_size, Queue* queue, char* outputName) {
    int output_fd = open(outputName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // BMP 헤더와 색상 테이블 출력
    write(output_fd, fileHeader, sizeof(BmpHeader));
    write(output_fd, color_table, color_table_size);

    // 큐에서 데이터를 하나씩 꺼내서 파일에 기록
    Node* current = queue->front;  // current는 Node* 타입이어야 함
    while (current != NULL) {
        // printf("Write: %d\n", current->data);
        write(output_fd, &current->data, sizeof(current->data));  // 큐에서 데이터 하나씩 꺼내서 파일에 씀
        Node* temp = current;
        current = current->next;  // current는 Node* 타입으로 수정
        free(temp);  // 사용한 메모리 해제
    }

    // 파일 닫기
    close(output_fd);
}


int main(int argc, char* argv[]) {

    int n = atoi(argv[1]);
    int open_fd = open(argv[2], O_RDONLY);
    char* outputName = argv[3];

    lseek(open_fd, 0, SEEK_SET);

    // -----------공유 queue 생성 ----------

    Queue queue;
    init_queue(&queue);

    // ---------------쓰레드 생성 --------------
    pthread_t threads[n]; //n개의 쓰레드 생성

    // ------------- bmp 헤더파일 ----------------
    BmpHeader fileHeader;
    read_header(&fileHeader, open_fd);
    // ------------ 색상 테이블 부분 ------------------
    int color_table_size = fileHeader.bfOffBits - sizeof(fileHeader); //색상 테이블 크기 구하기
    char* color_table = (char *)malloc(color_table_size); // 색상 테이블 메모리 할당
    
    lseek(open_fd, sizeof(BmpHeader), SEEK_SET); // 헤더 이후 색상 테이블 시작 위치
    read(open_fd, color_table, color_table_size);    

    // ------------- 픽셀 데이터 길이 --------------
    lseek(open_fd, 0, SEEK_SET);
    lseek(open_fd, fileHeader.bfOffBits, SEEK_SET);

    // ------------- 픽셀 데이터 부분 ---------------
    int open_id[n];
    ThreadArgv threadArgs[n];
    int threadOffBits = (fileHeader.bfSize - fileHeader.bfOffBits) / n;
    // printf("bfoff: %d, fullSize: %d\n", fileHeader.bfOffBits, fileHeader.bfSize);

    for (int i = 0; i < n; i++) {
        open_id[i] = open(argv[2], O_RDONLY);
        threadArgs[i].queue = &queue;
        threadArgs[i].thread_id = i;
        threadArgs[i].strBfOffBits = fileHeader.bfOffBits + i * threadOffBits;
        threadArgs[i].calNum = threadOffBits;
        threadArgs[i].filesize = fileHeader.bfSize;
        threadArgs[i].open_fd = open_id[i];
       
        pthread_create(&threads[i], NULL, thread_cal, (void*)&threadArgs[i]);
    }

    for (int i = 0; i < n; i++) { //쓰레드가 작업을 완료할때까지 대기
        pthread_join(threads[i], NULL);
    }

    close(open_fd);

    // ---------------- output.bmp만들기 -------------
    made_output(&fileHeader, color_table, fileHeader.bfSize,  color_table_size, &queue, outputName);

    free(color_table);

    return 0;

    
}

