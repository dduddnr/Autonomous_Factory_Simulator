#include <stdio.h>      // 표준 입출력 (printf, perror)
#include <stdlib.h>     // 표준 유틸리티 (exit)
#include <string.h>     // 문자열 처리 (strlen)
#include <unistd.h>     // Unix 표준 (close, read, write)
#include <arpa/inet.h>  // 인터넷 프로토콜 (inet_pton)
#include <sys/socket.h> // 소켓 통신 (socket, connect)

#define PORT 8080           // 서버가 기다리는 포트 번호 (서버와 동일해야 함)
#define SERVER_IP "127.0.0.1" // 서버의 IP 주소 (여기서는 자기 자신, 즉 localhost)

int main() {
    int sock = 0; // 클라이언트 소켓 파일 디스크립터
    struct sockaddr_in serv_addr; // 서버 주소 정보를 담을 구조체
    const char *message = "ARM01:OK:100\n"; // 서버로 보낼 메시지 (프로토콜 예시)

    // 1. 소켓 생성: IPv4, TCP (SOCK_STREAM) 소켓을 만듭니다.
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // IP 주소 변환: 문자열 IP 주소(SERVER_IP)를 네트워크 주소로 변환
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("유효하지 않은 주소 / 주소 변환 실패");
        exit(EXIT_FAILURE);
    }

    // 2. 서버에 연결 (connect)
    // sock: 연결할 클라이언트 소켓
    // (struct sockaddr *)&serv_addr: 연결할 서버 주소 정보
    // sizeof(serv_addr): 주소 구조체의 크기
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("서버 연결 실패");
        exit(EXIT_FAILURE);
    }
    printf("서버에 성공적으로 연결되었습니다.\n");

    // 3. 서버로 데이터 쓰기 (write)
    // sock: 서버와 통신하는 소켓
    // message: 보낼 데이터
    // strlen(message): 보낼 데이터의 길이
    write(sock, message, strlen(message));
    printf("서버로 메시지 전송: %s", message);

    // 4. 소켓 닫기
    close(sock);
    printf("클라이언트 종료.\n");
    return 0;
}