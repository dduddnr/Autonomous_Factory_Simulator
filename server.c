#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ncurses.h> // TUI 라이브러리

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_CLIENTS 10 // 최대 접속 가능한 기계 수

// [공유 데이터] 모든 스레드가 이 변수를 함께 씁니다.
char machine_status[MAX_CLIENTS][BUF_SIZE]; // 각 기계의 상태 메시지 저장
int active_clients[MAX_CLIENTS] = {0};      // 접속 여부 (0: 끊김, 1: 연결됨)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

const char *SENSOR_IDS[MAX_CLIENTS] = { //ui에서 각 클라이언트가 특정 센서이므로 맞춤 프로토콜 필요 -> 센서마다 인덱스 고정 필요
    "ARM01",    // index 0
    "TEMP02",   // index 1
    "BUTTON01", // index 2
    "LED01",    // index 3
    "sensor01", // index 4
    "sensor02", // index 5
    "sensor03", // index 6
    "sensor04", // index 7
    "sensor05", // index 8
    "sensor06", // index 9
};

// [UI 스레드] 0.1초마다 공유 데이터를 읽어서 화면을 새로 그립니다.
void *draw_ui_thread(void *arg) {
    initscr();              // ncurses 시작
    curs_set(0);            // 커서 숨김
    noecho();               // 키 입력 화면 노출 방지
    start_color();          // 색상 모드 시작

    // 색상 정의 (번호, 글자색, 배경색)
    init_pair(1, COLOR_CYAN, COLOR_BLACK);  // 제목 (하늘색)
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // 클라이언트 연결됨 (초록색)
    init_pair(3, COLOR_RED, COLOR_BLACK);   // 클라이언트 끊김 (빨간색)

    while (1) {
        clear(); // 화면 지우기

        // 1. 제목 그리기
        attron(COLOR_PAIR(1));
        mvprintw(1, 2, "========================================");
        mvprintw(2, 2, "   FACTORY MONITORING SYSTEM (v2.0)   ");
        mvprintw(3, 2, "========================================");
        attroff(COLOR_PAIR(1));

        pthread_mutex_lock(&lock);
        // 2. 기계 상태 목록 그리기
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int row = 6 + i; // 6번째 줄부터 한 줄씩 출력
            
            if (active_clients[i]) {
                // 접속된 경우
                attron(COLOR_PAIR(2)); // 초록색
                mvprintw(row, 2, "[Machine %s] Status: %s", SENSOR_IDS[i], machine_status[i]); //프로토콜 구체화 필요
                attroff(COLOR_PAIR(2));
            } else {
                // 접속 안 된 경우
                attron(COLOR_PAIR(3)); // 빨간색
                mvprintw(row, 2, "[Machine %s] Waiting for connection...", SENSOR_IDS[i]); //프로토콜 구체화 필요
                attroff(COLOR_PAIR(3));
            }
        }
        pthread_mutex_unlock(&lock);

        // 3. 안내 문구
        mvprintw(20, 2, "Listening on Port %d...", PORT);
        mvprintw(21, 2, "Press Ctrl+C to exit server.");

        refresh();      // 실제 화면 업데이트
        usleep(100000); // 0.1초 휴식 (CPU 과부하 방지)
    }

    endwin();
    return NULL;
}

// [작업 스레드] 클라이언트와 1:1로 대화하며 데이터를 공유 메모리에 적습니다.
void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);

    char buffer[BUF_SIZE];
    int str_len;
    int id = -1; //초기화 (못 찾음 : -1)
    int has_sent_msg = 0; //센서 ID 확인용 플래그

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        str_len = read(client_sock, buffer, BUF_SIZE);

        if (str_len <= 0) break; // 연결 종료
        
        buffer[strcspn(buffer, "\n")] = 0; // 개행 문자 제거 (화면 깨짐 방지)

        if (id == -1) { //ui 상에서 아직 자리가 배정 안 됐다면
            char temp_id[20]; 
            sscanf(buffer, "%[^:]", temp_id); // 메시지에서 ':' 앞부분까지만 복사해서 temp_id에 저장

            for(int i=0; i<MAX_CLIENTS; i++) { // 명단(SENSOR_IDS)을 탐색해 자리를 찾는다
                if(SENSOR_IDS[i] != NULL && strcmp(temp_id, SENSOR_IDS[i]) == 0) {
                    id = i; //자리를 찾음
                    
                    pthread_mutex_lock(&lock);
                    active_clients[id] = 1; 
                    pthread_mutex_unlock(&lock);
                    break;
                }
            }

            //ID 확인 결과에 따라 답장 보내기
            if (id == -1) { 
                printf("Unknown Sensor Rejected: %s\n", buffer); //존재하지 않는 센서 ID
                char *msg = "DENIED";
                write(client_sock, msg, strlen(msg));
                break; // 루프 탈출 -> 연결 종료
            } else {
                if (!has_sent_msg) { //센서 ID 존재함 -> 승인 메시지 전송 (최초 1회만)
                    pthread_mutex_lock(&lock);
                    active_clients[id] = 1; 
                    pthread_mutex_unlock(&lock);
                    
                    char *msg = "ACCEPTED";
                    write(client_sock, msg, strlen(msg));
                    has_sent_msg = 1;
                }
            }
        }

        pthread_mutex_lock(&lock);
        snprintf(machine_status[id], BUF_SIZE, "%s", buffer);
        pthread_mutex_unlock(&lock);
    }

    if (id != -1) { //연결 종료 처리
        pthread_mutex_lock(&lock);
        active_clients[id] = 0; 
        pthread_mutex_unlock(&lock);
    }
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t t_id, ui_tid;

    // 소켓 생성 및 설정
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error"); exit(1);
    }
    if (listen(server_sock, 5) == -1) {
        perror("listen error"); exit(1);
    }

    // [핵심] UI 스레드 별도 실행
    pthread_create(&ui_tid, NULL, draw_ui_thread, NULL);
    pthread_detach(ui_tid);

    // 메인 스레드는 계속 접속만 받음
    while (1) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        
        if (client_sock == -1) continue;

        int *new_sock = (int *)malloc(sizeof(int));
        *new_sock = client_sock;

        // 클라이언트마다 작업 스레드 생성
        pthread_create(&t_id, NULL, handle_client, (void *)new_sock);
        pthread_detach(t_id);
    }

    close(server_sock);
    return 0;
}