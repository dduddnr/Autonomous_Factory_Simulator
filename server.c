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

// [UI 스레드] 0.1초마다 공유 데이터를 읽어서 화면을 새로 그립니다.
void *draw_ui_thread(void *arg) {
    initscr();              // ncurses 시작
    curs_set(0);            // 커서 숨김
    noecho();               // 키 입력 화면 노출 방지
    start_color();          // 색상 모드 시작

    // 색상 정의 (번호, 글자색, 배경색)
    init_pair(1, COLOR_CYAN, COLOR_BLACK);  // 제목 (하늘색)
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // 연결됨 (초록색)
    init_pair(3, COLOR_RED, COLOR_BLACK);   // 끊김 (빨간색)

    while (1) {
        clear(); // 화면 지우기

        // 1. 제목 그리기
        attron(COLOR_PAIR(1));
        mvprintw(1, 2, "========================================");
        mvprintw(2, 2, "   FACTORY MONITORING SYSTEM (v2.0)   ");
        mvprintw(3, 2, "========================================");
        attroff(COLOR_PAIR(1));

        // 2. 기계 상태 목록 그리기
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int row = 6 + i; // 6번째 줄부터 한 줄씩 출력
            
            if (active_clients[i]) {
                // 접속된 경우
                attron(COLOR_PAIR(2)); // 초록색
                mvprintw(row, 2, "[Machine %d] Status: %s", i, machine_status[i]);
                attroff(COLOR_PAIR(2));
            } else {
                // 접속 안 된 경우
                attron(COLOR_PAIR(3)); // 빨간색
                mvprintw(row, 2, "[Machine %d] Waiting for connection...", i);
                attroff(COLOR_PAIR(3));
            }
        }
        
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

    // 편의상 소켓 번호를 인덱스로 사용 (범위 넘어가면 0번으로)
    int id = client_sock % MAX_CLIENTS; 
    
    // 접속 처리
    active_clients[id] = 1;
    snprintf(machine_status[id], BUF_SIZE, "Connected");

    char buffer[BUF_SIZE];
    int str_len;

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        str_len = read(client_sock, buffer, BUF_SIZE);

        if (str_len <= 0) break; // 연결 종료
        
        // 개행 문자 제거 (화면 깨짐 방지)
        buffer[strcspn(buffer, "\n")] = 0;

        // 받은 메시지를 공유 메모리에 업데이트 -> UI 스레드가 이걸 보고 그림
        snprintf(machine_status[id], BUF_SIZE, "%s", buffer);
    }

    // 연결 종료 처리
    active_clients[id] = 0;
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