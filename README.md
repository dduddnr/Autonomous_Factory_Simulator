# 실행 할 땐 sudo apt-get install libsdl2-dev libsdl2-mixer-dev를 실행시키고
# gcc server.c -o server -lncurses -lpthread -lSDL2 -lSDL2_mixer -DUSE_AUDIO로 다시 한 번 컴파일 해주세요.
# 아니면 효과음이 안 나와요.
# 깔기 싫으시면 gcc server.c -o server -lncurses -lpthread해서 효과음 없는 버전으로 해도 문제는 없습니다.

# 배경 효과음 출처: https://www.youtube.com/watch?v=-Ycu6uTPquc
# 혹시 잘 안 돌아갈 껄 대비해서 이 버전으로 업데이트 하기 전의 프로젝트를 백업해 두었으니 걱정은 마십시오.
