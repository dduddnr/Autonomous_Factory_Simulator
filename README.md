<!-- PROJECT LOGO & TITLE --><br /><div align="center"><a href="https://github.com/your_username/repo_name"><img src="https://www.google.com/search?q=https://cdn-icons-png.flaticon.com/512/3043/3043543.png" alt="Logo" width="80" height="80"></a><h3 align="center">Real-time Autonomous Factory Dashboard</h3><p align="center">TUI 기반 실시간 자율 공장 모니터링 시스템 (Team 16)<br /><br /><!-- DEMO LINK --><a href="#-실행-usage"><strong>View Demo »</strong></a></p></div>

<br /><!-- ABOUT THE PROJECT -->

## 📝 프로젝트 소개 (About The Project)이 프로젝트는 자율 공장의 중앙 관제 시스템을 시뮬레이션하는 시스템 프로그래밍 프로젝트입니다.AI 및 데이터 인프라의 핵심인 '대규모 데이터 실시간 수집 및 처리' 능력을 함양하기 위해, **C언어와 시스템 콜(System Call)**을 사용하여 백엔드 로직을 깊이 있게 구현했습니다. 관제 서버는 멀티스레드 환경에서 여러 센서(클라이언트)의 데이터를 동시에 수신하며, 이를 TUI(Text User Interface) 대시보드에 실시간으로 시각화하고 로그를 기록합니다.시스템 아키텍처 (System Architecture)Thread-per-Client 모델: accept() 후 클라이언트(센서)마다 별도의 Worker Thread를 생성하여 1:1 통신을 수행합니다.동시성 제어 (Concurrency Control): 여러 스레드가 공유 자원(TUI 데이터 구조체, 로그 파일)에 접근할 때 발생할 수 있는 경쟁 상태(Race Condition)를 방지하기 위해 Mutex를 사용하여 원자성(Atomicity)을 보장합니다.System Calls: socket, bind, listen, accept, open, write, read 등의 시스템 콜을 직접 사용하여 저수준 I/O를 제어합니다.

<br /><!-- KEY FEATURES -->

## ✨ 주요 기능 (Key Features)제안서에 명시된 5가지 핵심 기능을 모두 구현했습니다.다중 센서 연결 수용 (Multi-Client Support)여러 대의 센서가 동시에 접속 가능하며, pthread를 이용해 각 센서별로 독립적인 통신을 유지합니다.실시간 TUI 대시보드 (Real-time Visualization)접속된 센서들의 상태를 ncurses 라이브러리를 통해 터미널 화면에 실시간으로 시각화합니다.별도의 렌더링 스레드가 공유 데이터를 읽어 화면을 갱신합니다.이벤트 로깅 (History Logging)수신된 모든 데이터를 factory.log 파일에 영구적으로 기록합니다.파일 접근 시 Mutex 동기화를 통해 데이터 무결성을 보장합니다.원격 장치 제어 (Bi-directional Communication)서버에서 특정 센서에게 RESET, LED_ON 등의 제어 명령을 전송하는 양방향 통신을 지원합니다.오류 실시간 경보 (Real-time Alerting)'ERROR' 상태 감지 시 화면 깜빡임 효과와 함께 경고음(SDL2)을 재생하여 관리자에게 즉각적인 알림을 제공합니다.

<br /><!-- GETTING STARTED -->

## 🚀 빌드 및 실행 방법 (Build & Run)이 프로젝트를 로컬 환경(WSL/Ubuntu)에서 실행하기 위한 가이드입니다.1. 전제 조건 (Prerequisites)이 프로그램은 **Ncurses(TUI)**와 SDL2(Audio Alert) 라이브러리를 사용합니다. 터미널에 다음 명령어를 입력하여 필요한 패키지를 설치하세요.sudo apt-get update
sudo apt-get install gcc make libncurses5-dev libncursesw5-dev libsdl2-dev libsdl2-mixer-dev
2. 설치 및 빌드 (Installation)레포지토리를 클론하고 make 명령어로 전체 기능을 빌드합니다.
1. Clone the repository
git clone [https://github.com/your_username/repo_name.git](https://github.com/your_username/repo_name.git)

2. Go to project folder
cd repo_name

3. Build (Includes Audio)
make
⚠️ 문제 해결 (Troubleshooting):만약 SDL2 오디오 라이브러리 문제로 빌드가 실패한다면, 아래 명령어로 오디오 기능을 제외하고 수동 컴파일하세요.Server
gcc server.c -o server -lncurses -lpthread
Client
gcc client.c -o client -lncurses -lpthread
3. 실행 (Usage)Step 1. 관제 서버 실행서버를 먼저 실행하여 대시보드를 띄웁니다../server
Step 2. 센서 클라이언트 실행새로운 터미널 창을 열고 센서를 실행하여 데이터를 전송합니다. (여러 터미널에서 실행 가능)./client
Step 3. 종료서버나 클라이언트 터미널에서 Ctrl + C 입력

<br /><!-- TEAM INFO -->

## 👥 팀원 정보 (Team Info)RoleNameStudent IDTeam 16노치승2021110721Team Member이영욱2022113756Team Member정현호2023025941Project Link: https://github.com/your_username/repo_name
