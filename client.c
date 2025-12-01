#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // 인터넷 프로토콜 (inet_pton)
#include <sys/socket.h> // 소켓 통신 (socket, connect)
#include <pthread.h>    // 서버 강제 종료시 종료용 (pthread_create)
#include <signal.h>

#define PORT 8080             // 서버가 기다리는 포트 번호 (서버와 동일해야 함)
#define SERVER_IP "127.0.0.1" // 서버의 IP 주소 (여기서는 자기 자신, 즉 localhost)

char my_id[20]; // 내 ID 저장 (예: ARM01)

void *recv_thread(void *s) {
    int sock = *(int *)s;
    char buffer[100];
    while (1) {
        int len = read(sock, buffer, sizeof(buffer)-1);
        if (len <= 0) { //서버 연결 끊김
            printf("서버와 연결이 끊어졌습니다.\n");
            kill(getpid(), SIGINT);
            break;
        }
        buffer[len] = 0;
        printf("[Server]: %s\n", buffer); // 서버 명령 수신용 (나중에 사용)
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { //실행할 때 센서 ID를 입력받음 (./client sensor01)
        printf("사용법: %s <SensorID>\n", argv[0]);
        printf("예시: %s ARM01\n", argv[0]);
        return 1;
    }
    strcpy(my_id, argv[1]); //sensor ID 저장

    int sock;
    struct sockaddr_in serv_addr;
    char message[256];
    pthread_t t_id;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        return 1;
    }

    printf("서버 접속 성공! sensor ID 확인 중...\n");
    sprintf(message, "%s:CONNECTED", my_id);
    write(sock, message, strlen(message));

    char auth_buffer[100];
    int len = read(sock, auth_buffer, sizeof(auth_buffer)-1);
    
    if (len > 0) {
        auth_buffer[len] = 0;
        if (strcmp(auth_buffer, "DENIED") == 0) { //서버에서 확인되지 않는 sensor ID
            printf("\n[오류] 존재하지 않는 센서 ID입니다: %s\n", my_id);
            printf("올바른 ID로 다시 실행해주세요.\n");
            close(sock);
            return 0;
        } else if (strcmp(auth_buffer, "ACCEPTED") == 0) { //서버에서 확인된 sensor ID
            printf("[성공] 서버 인증 완료! 데이터 전송을 시작합니다.\n");
        }
    } else {
        printf("서버로부터 응답이 없습니다.\n");
        close(sock);
        return 0;
    }

    // 수신 스레드 생성 (서버가 보내는 명령을 듣기 위해)
    pthread_create(&t_id, NULL, recv_thread, (void *)&sock);
    pthread_detach(t_id);

    while (1) {
        // 사용자 입력 대기
        printf("상태 입력 (예: OK, ERROR): "); //센서에 특정 조건이 걸리면 에러를 출력하도록 하기 위함
        if (fgets(message, sizeof(message), stdin) == NULL) break;
        
        message[strcspn(message, "\n")] = 0;

        if (strlen(message) == 0) continue;

        // 보낼 때도 자동으로 "ID:입력값" 형태로 전송
        char full_msg[300];
        sprintf(full_msg, "%s:%s", my_id, message);
        write(sock, full_msg, strlen(full_msg));
    }

    close(sock);
    return 0;
}