#include <stdio.h>      // 표준 입출력 (printf, perror)
#include <stdlib.h>     // 표준 유틸리티 (exit)
#include <string.h>     // 문자열 처리 (memset)
#include <unistd.h>     // Unix 표준 (close, read, write)
#include <arpa/inet.h>  // 인터넷 프로토콜 (inet_pton, ntohs)
#include <sys/socket.h> // 소켓 통신 (socket, bind, listen, accept)

#define PORT 8080       // 서버가 클라이언트의 연결을 기다릴 포트 번호
#define BUFFER_SIZE 1024 // 데이터 수신 버퍼 크기

int main() {
    int server_fd, new_socket; // server_fd: 서버 소켓 파일 디스크립터, new_socket: 클라이언트와 통신할 소켓
    struct sockaddr_in address; // 서버 주소 정보를 담을 구조체
    int addrlen = sizeof(address); // 주소 구조체 크기
    char buffer[BUFFER_SIZE] = {0}; // 클라이언트로부터 데이터를 받을 버퍼

    // 1. 소켓 생성: IPv4, TCP (SOCK_STREAM) 소켓을 만듭니다.
    // server_fd = socket(AF_INET, SOCK_STREAM, 0)
    // AF_INET: IPv4 인터넷 프로토콜
    // SOCK_STREAM: 연결 지향형 소켓 (TCP)
    // 0: 기본 프로토콜 (TCP의 경우)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    // address.sin_family: 주소 체계 (IPv4)
    // address.sin_addr.s_addr: 서버 IP 주소 (INADDR_ANY는 모든 인터페이스의 IP를 사용)
    // address.sin_port: 서버 포트 번호 (htons는 호스트 바이트 순서를 네트워크 바이트 순서로 변환)
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 2. 소켓에 IP 주소와 포트 번호 할당 (bind)
    // bind(server_fd, (struct sockaddr *)&address, addrlen)
    // server_fd: 바인딩할 소켓
    // (struct sockaddr *)&address: 바인딩할 주소 정보 (제네릭 소켓 주소 구조체로 형변환)
    // addrlen: 주소 구조체의 크기
    if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) {
        perror("바인딩 실패");
        exit(EXIT_FAILURE);
    }

    // 3. 클라이언트의 연결 요청을 기다림 (listen)
    // listen(server_fd, 3)
    // server_fd: 리슨할 소켓
    // 3: 대기 큐의 최대 크기 (동시에 처리할 수 있는 연결 요청 수)
    if (listen(server_fd, 3) < 0) {
        perror("리슨 실패");
        exit(EXIT_FAILURE);
    }

    printf("서버가 %d 포트에서 클라이언트 연결을 기다리는 중...\n", PORT);

    // 4. 클라이언트의 연결 요청 수락 (accept)
    // 이 함수는 클라이언트가 연결을 요청할 때까지 블로킹됩니다.
    // 연결이 수락되면, 클라이언트와 통신할 새로운 소켓 디스크립터를 반환합니다.
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("연결 수락 실패");
        exit(EXIT_FAILURE);
    }
    printf("클라이언트가 연결되었습니다.\n");

    // 5. 클라이언트로부터 데이터 읽기 (read)
    // new_socket: 클라이언트와 통신하는 소켓
    // buffer: 데이터를 저장할 버퍼
    // BUFFER_SIZE - 1: 버퍼 크기 (널 종료 문자를 위해 1바이트 남김)
    int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
    if (valread > 0) {
        buffer[valread] = '\0'; // 문자열 끝을 널 종료
        printf("클라이언트로부터 받은 메시지: %s\n", buffer);
    } else {
        printf("클라이언트로부터 데이터를 받지 못했습니다.\n");
    }

    // 6. 소켓 닫기
    close(new_socket); // 클라이언트와 통신했던 소켓 닫기
    close(server_fd);  // 서버 리스닝 소켓 닫기

    printf("서버 종료.\n");
    return 0;
}