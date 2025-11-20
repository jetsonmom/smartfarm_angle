# smartfarm_angle

JetPack 4.6에서 이 코드를 실행하기 전에 필요한 라이브러리 설치 가이드입니다:

## 1. 시스템 업데이트
```bash
sudo apt update
sudo apt upgrade -y
```

## 2. Python3 및 pip 설치 확인
```bash
sudo apt install -y python3 python3-pip
python3 --version
pip3 --version
```

## 3. OpenCV 설치

JetPack 4.6에는 OpenCV가 포함되어 있지만, Python 바인딩 확인:

```bash
# 이미 설치된 OpenCV 확인
python3 -c "import cv2; print(cv2.__version__)"
```

만약 에러가 나면:
```bash
sudo apt install -y python3-opencv
```

또는 pip로 설치:
```bash
pip3 install opencv-python
```

## 4. Schedule 라이브러리 설치
```bash
pip3 install schedule
```

## 5. V4L2 관련 패키지 설치 (카메라 지원)
```bash
sudo apt install -y v4l-utils
```

## 6. 카메라 권한 설정
```bash
sudo usermod -a -G video $USER
# 재부팅 필요
sudo reboot
```

## 7. 전체 설치 스크립트

한 번에 실행:
```bash
#!/bin/bash

# 시스템 업데이트
sudo apt update && sudo apt upgrade -y

# 필수 패키지
sudo apt install -y python3 python3-pip v4l-utils

# Python 라이브러리
pip3 install schedule opencv-python

# 카메라 권한
sudo usermod -a -G video $USER

echo "✅ 설치 완료! 재부팅 후 사용하세요."
echo "재부팅: sudo reboot"
```

## 8. 설치 확인

```bash
# OpenCV 확인
python3 -c "import cv2; print('OpenCV:', cv2.__version__)"

# Schedule 확인
python3 -c "import schedule; print('Schedule: OK')"

# 카메라 확인
v4l2-ctl --list-devices
```

## 9. Gmail 앱 비밀번호 설정

코드의 `EMAIL_CONFIG`에서 비밀번호 설정 필요:
1. Google 계정 → 보안 → 2단계 인증 활성화
2. "앱 비밀번호" 생성
3. 생성된 16자리 비밀번호를 코드에 입력

## 10. 스크립트 실행

```bash
# 실행 권한 부여
chmod +x plant_monitor.py

# 실행
python3 plant_monitor.py

# 백그라운드 실행 (선택)
nohup python3 plant_monitor.py > /dev/null 2>&1 &
```

설치 중 문제가 있으면 알려주세요! 🌱
