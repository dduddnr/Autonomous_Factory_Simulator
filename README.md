<!-- PROJECT LOGO & TITLE -->

<br />
<div align="center">
<h3 align="center">Real-time Autonomous Factory Dashboard</h3>

<p align="center">
TUI 기반 실시간 자율 공장 모니터링 시스템 (Team 16)
</p>
</div>
<br />
<!-- ABOUT THE PROJECT -->

# 📝 프로젝트 소개 (About The Project)
이 프로젝트는 자율 공장의 중앙 관제 시스템을 시뮬레이션하는 시스템 프로그래밍 프로젝트입니다.

AI 및 데이터 인프라의 핵심인 '대규모 데이터 실시간 수집 및 처리' 기능을 C언어와 시스템 콜(System Call)을 사용하여 구현했습니다. 

관제 서버는 멀티스레드 환경에서 여러 센서(클라이언트)의 데이터를 동시에 수신하며 이를 TUI(Text User Interface) 대시보드에 실시간으로 시각화하고 로그를 기록합니다.

## 시스템 아키텍처 (System Architecture)

Thread-per-Client 모델: accept() 후 클라이언트(센서)마다 별도의 Worker Thread를 생성하여 1:1 통신을 수행합니다.

동시성 제어 (Concurrency Control): 여러 스레드가 공유 자원(TUI 데이터 구조체, 로그 파일)에 접근할 때 발생할 수 있는 경쟁 상태(Race Condition)를 방지하기 위해 Mutex를 사용하여 원자성(Atomicity)을 보장합니다.

System Calls: socket, bind, listen, accept, open, write, read 등의 시스템 콜을 직접 사용하여 저수준 I/O를 제어합니다.

<!-- KEY FEATURES -->

# ✨ 주요 기능 (Key Features)
제안서에 명시된 5가지 핵심 기능을 모두 구현했습니다.

## 다중 센서 연결 수용 (Multi-Client Support)

여러 대의 센서가 동시에 접속 가능하며, pthread를 이용해 각 센서별로 독립적인 통신을 유지합니다.

## 실시간 TUI 대시보드 (Real-time Visualization)

접속된 센서들의 상태를 ncurses 라이브러리를 통해 터미널 화면에 실시간으로 시각화합니다.

별도의 렌더링 스레드가 공유 데이터를 읽어 화면을 갱신합니다.

## 이벤트 로깅 (History Logging)

수신된 모든 데이터를 factory.log 파일에 영구적으로 기록합니다.

파일 접근 시 Mutex 동기화를 통해 데이터 무결성을 보장합니다.

## 원격 장치 제어 (Bi-directional Communication)

서버에서 특정 센서에게 RESET, LED ON 등의 제어 명령을 전송하는 양방향 통신을 지원합니다.

## 오류 실시간 경보 (Real-time Alerting)

ERROR 상태 감지 시 화면 깜빡임 효과와 함께 경고음(SDL2)을 재생하여 관리자에게 즉각적인 알림을 제공합니다.

<데모 영상>

1.  https://youtu.be/zjQvuDtMkI8?si=i59E9MxsYkm4PI0R
2. https://youtu.be/5ia1xZpAyXI?si=47uz3IfdclxaRDse
3. https://youtube.com/shorts/FbQkM-IymSo?si=fi4fPhLOZUB_cg6c
<!-- GETTING STARTED -->

# 🚀 빌드 및 실행 방법 (Build & Run)
이 프로젝트를 로컬 환경(WSL/Ubuntu)에서 실행하기 위한 가이드입니다.

## 1. 전제 조건 (Prerequisites)

이 프로그램은 ncurses(TUI)와 SDL2(Audio Alert) 라이브러리를 사용합니다. 터미널에 다음 명령어를 입력하여 필요한 패키지를 설치하세요.

sudo apt-get update

sudo apt-get install gcc make libncurses5-dev libncursesw5-dev libsdl2-dev libsdl2-mixer-dev


## 2. 설치 및 빌드 (Installation)

레포지토리를 클론하고 make 명령어로 전체 기능을 빌드합니다.

1. Clone the repository

2. Go to project folder

3. Build (Includes Audio) : make

⚠️ 문제 해결 (Troubleshooting):
만약 SDL2 오디오 라이브러리 문제로 빌드가 실패한다면, 아래 명령어로 오디오 기능을 제외하고 수동 컴파일하세요.

Server

-gcc server.c -o server -lncurses -lpthread

Client

-gcc client.c -o client -lncurses -lpthread


# 🔌 하드웨어 구성 (Hardware Setup)
본 프로젝트의 Client는 Raspberry Pi 5 환경에서 동작하며, GPIO와 커널 드라이버를 통해 하드웨어를 제어합니다. 아래 핀맵에 따라 회로를 구성하고 시스템 설정을 완료해야 합니다.

## 1. 회로 연결 (Pin Map)

| 컴포넌트 (Component) | 핀 (Pin / BCM) | 연결 설명 (Connection) |

| **DHT11 (Temp/Humi)** | **GPIO 4** | Data Pin 연결 (VCC: 3.3V, GND: GND) |
| **Button (Switch)** | **GPIO 17** | 한쪽은 GPIO 17, 다른 쪽은 3.3V에 연결*(Internal Pull-down 설정 사용)* |
| **LED** | **GPIO 18** | Anode(+)는 GPIO 18, Cathode(-)는 GND*(저항 220Ω 직렬 연결 권장)* |

## 2. 시스템 설정 (System Configuration) - 필수

DHT11 센서 데이터를 커널 드라이버(`/sys/bus/iio/devices`)를 통해 읽어오기 위해, 라즈베리 파이 부트 설정에 오버레이를 추가해야 합니다.

1. **설정 파일 열기**
터미널에서 아래 명령어를 입력하여 `config.txt` 파일을 엽니다.
   sudo nano /boot/firmware/config.txt


## 3. 실행 (Usage)

본 프로젝트는 라즈베리 파이(Raspberry Pi) 환경을 기준으로 개발되었으나 개발 편의를 위해 WSL(Windows Subsystem for Linux) 환경에서도 시뮬레이션이 가능하도록 설정이 필요합니다.

WSL 환경에서 실행하기 전에 아래 [Step 0]의 과정을 반드시 먼저 수행해주세요.

### Step 0. WSL 환경 사전 설정 (최초 1회 필수)

PC(WSL)에는 라즈베리 파이와 달리 GPIO 핀을 제어하는 하드웨어가 없습니다. 따라서 코드 실행 시 발생하는 pinctrl not found 오류를 방지하기 위해 가짜(Dummy) 명령어를 생성해야 합니다.

터미널에 다음 명령어들을 차례대로 복사하여 입력해 주세요.

1. 가짜 pinctrl 스크립트 생성 (내용: #!/bin/bash)

echo '#!/bin/bash' | sudo tee /usr/bin/pinctrl


2. 스크립트 종료 코드 추가 (내용: exit 0 - 항상 성공으로 처리)

echo 'exit 0' | sudo tee -a /usr/bin/pinctrl


3. 실행 권한 부여

sudo chmod +x /usr/bin/pinctrl

### Step 1. 관제 서버 실행


서버를 먼저 실행하여 대시보드를 띄웁니다.

./server

확인: "FACTORY MONITORING SYSTEM" 로고와 함께 Waiting for connection... 화면이 나타납니다.



### Step 2. 센서 클라이언트 실행


새로운 터미널 창을 열고 클라이언트를 실행합니다. 이 프로그램은 4개의 가상 장치(ARM01, TEMP02, BUTTON01, LED01)를 동시에 구동합니다.

라즈베리 파이(GPIO 제어 권한 필요): sudo ./client

WSL (시뮬레이션): ./client

확인: 서버 대시보드의 [Machine ARM01], [TEMP02], [BUTTON01], [LED01] 상태바가 초록색으로 활성화됩니다.


### Step 3. 핵심 기능 검증


서버와 클라이언트가 연결된 상태에서 다음 기능들을 테스트할 수 있습니다.


1. 원격 장치 제어 (Remote Control)

서버 터미널에서 RESET 입력 후 [Enter].

방향키(↑/↓)로 클라이언트를 선택 후 [Enter].

결과: 클라이언트 측 LED가 3회 깜빡이며 리부팅 시퀀스가 작동합니다.

서버에서 입력한 명령어가 RESET이 아닐 경우에는 해당 클라이언트(센서) 터미널에 명령어를 출력합니다.


2. 비상 정지 및 경보 (Emergency Stop)

클라이언트(라즈베리 파이)의 **물리 버튼(GPIO 17)**을 누릅니다.

결과: 서버 대시보드에 WARNING - INTERRUPT DETECTED! 경고가 표시되고, 현장의 LED가 즉시 꺼집니다.


3. 로그 확인 (Logging)

새 터미널을 열어 실시간 로그를 확인합니다.

tail -f factory.log


### Step 4. 종료


서버나 클라이언트 터미널 어디서든 종료 신호를 입력하면 시스템이 안전하게 정지합니다.

입력: Ctrl + C

동작: 서버 종료 시 모든 클라이언트가 이를 감지하고 자동으로 안전 종료(Safe Shutdown) 되며 LED가 소등됩니다.



<!-- TEAM INFO -->

# 👥 팀원 정보 (Team Info)
Team 16

팀원 1: 노치승 / 2021110721

팀원 2: 이영욱 / 2022113756

팀원 3: 정현호 / 2023025941
