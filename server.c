#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <ncurses.h> // TUI 라이브러리
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define BUF_SIZE 1024
#define LOG_SIZE 4096
#define MAX_CLIENTS 10 // 최대 접속 가능한 기계 수

// [공유 데이터] 모든 스레드가 이 변수를 함께 씁니다.
char machine_status[MAX_CLIENTS][BUF_SIZE], time_buffer[BUF_SIZE],
    log_buffer[LOG_SIZE]; // 각 기계의 상태 메시지 저장
int active_clients[MAX_CLIENTS] = {0}, client_sockss[MAX_CLIENTS],
    client_error[MAX_CLIENTS] = {0},
    log_fd = 0; // 접속 여부 (0: 끊김, 1: 연결됨)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct tm time_struct;
time_t t;

const char *SENSOR_IDS[MAX_CLIENTS] = {
    // ui에서 각 클라이언트가 특정 센서이므로 맞춤 프로토콜 필요 -> 센서마다
    // 인덱스 고정 필요
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

void play_intro() {
  char buffer[40][165];
  FILE *f;
  if (!(f = fopen("intro.txt", "r"))) {
    return;
  }
  int inner_timer1, inner_timer2;
  int font_color = 0;
  // 인트로 글자 읽기
  for (int j = 0; j < 40; j++) {
    fread(buffer[j], sizeof(char), 164, f);
    buffer[j][164] = '\0';
  }
  fclose(f);

  for (int i = 0; i < 150; i++) {
    // 인트로 글자 색깔 설정
    inner_timer1 = (i > 20) ? i - 20 : 0;
    font_color =
        (255 >= 232 + (inner_timer1 / 4)) ? 232 + (inner_timer1 / 4) : 255;
    init_pair(10, font_color, COLOR_BLACK);
    attron(COLOR_PAIR(10));

    // 인트로는 미완성 상태.
    for (int i = 0; i < 40; i++) {
      for (int j = 0; j < 163; j++) {
        mvaddch(i + 4, j + 8, buffer[i][j]);
      }
    }

    attroff(COLOR_PAIR(10));
    refresh();
    usleep(100000);
  }
  endwin();
  printf("\e[8;23;60t");
  fflush(stdout);
  usleep(100000);
  initscr();
}

void send_command(char command[]) {
  int ch, select = 0;
  clear();

  attron(COLOR_PAIR(1));
  mvprintw(1, 10, "========================================");
  mvprintw(2, 10, "           SELECT THE CLIENT            ");
  mvprintw(3, 10, "========================================");
  attroff(COLOR_PAIR(1));

  pthread_mutex_lock(&lock);
  // 2. 기계 상태 목록 그리기
  for (int i = 0; i < MAX_CLIENTS; i++) {
    int row = 6 + i; // 6번째 줄부터 한 줄씩 출력

    if (active_clients[i]) {
      // 접속된 경우
      attron(COLOR_PAIR(4));
      mvprintw(row, 2, "[Machine %s]", SENSOR_IDS[i]); // 프로토콜 구체화 필요
      attroff(COLOR_PAIR(4));
    } else {
      // 접속 안 된 경우
      attron(COLOR_PAIR(5));             // 회색
      mvprintw(row, 2, "Not available"); // 프로토콜 구체화 필요
      attroff(COLOR_PAIR(5));
    }
  }
  pthread_mutex_unlock(&lock);
  mvaddch(6, 59, '<');
  refresh();

  while (1) {
    while ((ch = getch()) != -1) {
      mvaddch(6 + select, 59, ' ');

      switch (ch) {
      case KEY_UP:
        if (!select)
          select = MAX_CLIENTS - 1;
        else
          select -= 1;
        break;
      case KEY_DOWN:
        select = (select + 1) % MAX_CLIENTS;
        break;
      case '\n':
      case '\r':
        move(6 + select, 0);
        clrtoeol();
        pthread_mutex_lock(&lock);

        gettime_log();
        if (!active_clients[select] ||
            (write(client_sockss[select], command, strlen(command)) == -1)) {
          attron(COLOR_PAIR(3));
          mvprintw(6 + select, 2, "Failed!");
          attroff(COLOR_PAIR(3));

          snprintf(log_buffer, LOG_SIZE,
                   "[%s] [INFO] Server has failed to send a command!\n",
                   time_buffer);
          write(log_fd, log_buffer, strlen(log_buffer));
        } else {
          attron(COLOR_PAIR(2));
          mvprintw(6 + select, 2, "Successfully sent!");
          attroff(COLOR_PAIR(2));

          snprintf(log_buffer, LOG_SIZE, "[%s] [MSG] To %s: %s\n", time_buffer,
                   SENSOR_IDS[select], command);
          write(log_fd, log_buffer, strlen(log_buffer));
        }

        pthread_mutex_unlock(&lock);
        refresh();
        sleep(1);
        return;
      }
    }

    mvaddch(6 + select, 59, '<');
  }
  refresh();
}

// [UI 스레드] 0.1초마다 공유 데이터를 읽어서 화면을 새로 그립니다.
void *draw_ui_thread(void *arg) {
  const char loading[4] = {'|', '/', '-', '\\'};
  const int red_fade[] = {160, 124, 88, 52, 0, 0, 0, 0};
  char command[32];
  int index = -1;
  int global_timer = 0;
  memset(command, 0, sizeof(command));
  int ch;
  initscr();     // ncurses 시작
  curs_set(0);   // 커서 숨김
  noecho();      // 키 입력 화면 노출 방지
  start_color(); // 색상 모드 시작
  keypad(stdscr, TRUE);
  timeout(10);

  // 색상 정의 (번호, 글자색, 배경색)
  init_pair(1, COLOR_CYAN, COLOR_BLACK); // 제목 (하늘색)
  init_pair(2, COLOR_GREEN, COLOR_BLACK); // 클라이언트 연결됨 (초록색)
  init_pair(3, COLOR_RED, COLOR_BLACK);   // 클라이언트 끊김 (빨간색)
  init_pair(4, COLOR_YELLOW, COLOR_BLACK);
  init_pair(5, 240, COLOR_BLACK);
  play_intro();

  while (1) {

    // 커맨드 입력 처리
    while ((ch = getch()) != -1) {
      switch (ch) {
      case '\n':
      case '\r':
        send_command(command);
        memset(command, 0, sizeof(command));
        index = -1;
        break;
      case KEY_BACKSPACE:
        if (index > -1)
          command[index--] = '\0';
        break;
      default:
        if ((index < 31) && (ch >= 32 && ch <= 126)) {
          command[++index] = (char)ch;
          command[index + 1] = '\0';
        }
      }
    }

    clear(); // 화면 지우기
    init_pair(9, COLOR_WHITE, red_fade[global_timer % 8]);

    // 1. 제목 그리기
    attron(COLOR_PAIR(1));
    mvprintw(1, 10, "========================================");
    mvprintw(2, 10, "       FACTORY MONITORING SYSTEM      ");
    mvprintw(3, 10, "========================================");
    attroff(COLOR_PAIR(1));

    pthread_mutex_lock(&lock);
    // 2. 기계 상태 목록 그리기
    for (int i = 0; i < MAX_CLIENTS; i++) {
      int row = 6 + i; // 6번째 줄부터 한 줄씩 출력

      if (active_clients[i]) { // 접속된 경우
        if (client_error[i])
          attron(COLOR_PAIR(9));
        else
          attron(COLOR_PAIR(2)); // 초록색
        mvprintw(row, 2, "[Machine %s] Status: %s", SENSOR_IDS[i],
                 machine_status[i]); // 프로토콜 구체화 필요
        attroff(COLOR_PAIR(9));
        attroff(COLOR_PAIR(2));
      } else {
        // 접속 안 된 경우
        attron(COLOR_PAIR(3)); // 빨간색
        mvprintw(row, 2, "[Machine %s] Waiting for connection.\t\t  %c",
                 SENSOR_IDS[i],
                 loading[(global_timer % 4)]); // 프로토콜 구체화 필요
        attroff(COLOR_PAIR(3));
      }
    }
    pthread_mutex_unlock(&lock);

    // 3. 안내 문구
    mvprintw(18, 2, "Command: ");
    mvprintw(18, 11, "%s", command);
    mvprintw(20, 2, "Listening on Port %d", PORT);
    for (int i = 0; i < (global_timer % 8) / 2; i++) {
      mvprintw(20, 24 + i, ".");
    }
    mvprintw(21, 2, "Press Ctrl+C to exit server.");

    refresh(); // 실제 화면 업데이트
    global_timer = (global_timer + 1) % 128;
    usleep(100000); // 0.1초 휴식 (CPU 과부하 방지)
  }

  endwin();
  return NULL;
}

void gettime_log() {
  t = time(NULL);
  localtime_r(&t,
              &time_struct); // 결과값을 내가 만든 변수 t에 담아줌 (안전)
  strftime(time_buffer, BUF_SIZE, "%Y-%m-%d %H:%M:%S", &time_struct);
}

// [작업 스레드] 클라이언트와 1:1로 대화하며 데이터를 공유 메모리에 적습니다.
void *handle_client(void *arg) {
  int client_sock = *((int *)arg);
  free(arg);

  char buffer[BUF_SIZE], temp_id[20], message[BUF_SIZE];
  int str_len;
  int id = -1;          // 초기화 (못 찾음 : -1)
  int has_sent_msg = 0; // 센서 ID 확인용 플래그

  while (1) {
    memset(buffer, 0, BUF_SIZE);
    str_len = read(client_sock, buffer, BUF_SIZE);

    if (str_len <= 0)
      break; // 연결 종료

    sscanf(buffer, "%[^:]:%[^\n]", temp_id, message);
    buffer[strcspn(buffer, "\n")] = 0;
    message[strcspn(buffer, "\n")] = 0;

    if (id == -1) { // ui 상에서 아직 자리가 배정 안 됐다면
      for (int i = 0; i < MAX_CLIENTS;
           i++) { // 명단(SENSOR_IDS)을 탐색해 자리를 찾는다
        if (SENSOR_IDS[i] != NULL && strcmp(temp_id, SENSOR_IDS[i]) == 0) {
          id = i; // 자리를 찾음

          pthread_mutex_lock(&lock);
          active_clients[id] = 1;
          client_sockss[id] = client_sock;
          pthread_mutex_unlock(&lock);
          break;
        }
      }

      // ID 확인 결과에 따라 답장 보내기
      if (id == -1) {
        printf("Unknown Sensor Rejected: %s\n", buffer); // 존재하지 않는 센서
                                                         // ID
        char *msg = "DENIED";
        write(client_sock, msg, strlen(msg));
        break; // 루프 탈출 -> 연결 종료
      } else {
        if (!has_sent_msg) { // 센서 ID 존재함 -> 승인 메시지 전송 (최초 1회만)
          pthread_mutex_lock(&lock);
          active_clients[id] = 1;
          pthread_mutex_unlock(&lock);

          char *msg = "ACCEPTED";
          write(client_sock, msg, strlen(msg));
          has_sent_msg = 1;
        }
      }
    }

    // 멀티쓰레드 서버용 안전한 버전
    pthread_mutex_lock(&lock);

    if (!strcmp("ERROR", message))
      client_error[id] = 1;
    else
      client_error[id] = 0;

    gettime_log();
    snprintf(log_buffer, LOG_SIZE, "[%s] [MSG] From %s: %s\n", time_buffer,
             temp_id, message);
    write(log_fd, log_buffer, strlen(log_buffer));

    snprintf(machine_status[id], BUF_SIZE, "%s", buffer);

    pthread_mutex_unlock(&lock);
  }

  if (id != -1) { // 연결 종료 처리
    pthread_mutex_lock(&lock);

    active_clients[id] = 0;
    gettime_log();
    snprintf(log_buffer, LOG_SIZE,
             "[%s] [INFO] Client [%s] has been disconnected.\n", time_buffer,
             temp_id);
    write(log_fd, log_buffer, strlen(log_buffer));
    pthread_mutex_unlock(&lock);
  }
  close(client_sock);
  return NULL;
}

void server_crashed() {
  pthread_mutex_lock(&lock);

  gettime_log();
  snprintf(log_buffer, LOG_SIZE,
           "[%s] [INFO] Server has been Ctrl^C'd. All the contents in the log "
           "will be deleted "
           "when the server is restarted.\n",
           time_buffer);
  write(log_fd, log_buffer, strlen(log_buffer));

  pthread_mutex_unlock(&lock);
  endwin();
  close(log_fd);
  signal(SIGINT, SIG_DFL);
  kill(getpid(), SIGINT);
}

int main() {
  signal(SIGINT, server_crashed);
  int server_sock, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  pthread_t t_id, ui_tid;
  if ((log_fd = open("factory.log", O_WRONLY | O_CREAT | O_TRUNC, 0644)) ==
      -1) {
    printf("Something's wrong with opening the log file.\n");
    exit(1);
  }

  // 소켓 생성 및 설정
  server_sock = socket(PF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind error");
    exit(1);
  }
  if (listen(server_sock, 5) == -1) {
    perror("listen error");
    exit(1);
  }
  // 쓰레드 만들기 전이기 때문에 락 필요없음.
  gettime_log();
  snprintf(log_buffer, LOG_SIZE, "[%s] [INFO] Server has started.\n",
           time_buffer);
  write(log_fd, log_buffer, strlen(log_buffer));

  printf("\e[8;48;180t");
  fflush(stdout);
  usleep(100000);
  // [핵심] UI 스레드 별도 실행
  pthread_create(&ui_tid, NULL, draw_ui_thread, NULL);
  pthread_detach(ui_tid);

  // 메인 스레드는 계속 접속만 받음
  while (1) {
    client_addr_size = sizeof(client_addr);
    client_sock =
        accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);

    if (client_sock == -1)
      continue;

    int *new_sock = (int *)malloc(sizeof(int));
    *new_sock = client_sock;

    pthread_mutex_lock(&lock);

    gettime_log();
    snprintf(log_buffer, LOG_SIZE,
             "[%s] [INFO] New client has tried to connect. Check the "
             "log right below for conformation.\n",
             time_buffer);
    write(log_fd, log_buffer, strlen(log_buffer));

    pthread_mutex_unlock(&lock);
    // 클라이언트마다 작업 스레드 생성
    pthread_create(&t_id, NULL, handle_client, (void *)new_sock);
    pthread_detach(t_id);
  }

  close(server_sock);
  return 0;
}