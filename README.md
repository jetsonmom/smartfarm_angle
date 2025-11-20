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
다시 정리하자면(에러가 나서 다시 정리)

네! 최종 완벽 가이드입니다! 🚀

## 📋 전체 설치 및 실행 가이드

### 1️⃣ 설치 스크립트 실행

```bash
# 설치 스크립트 생성
cat > install.sh << 'EOF'
#!/bin/bash
# 시스템 업데이트
sudo apt update
# 필수 패키지
sudo apt install -y python3-pip v4l-utils python3-opencv
# Schedule만 pip로 설치
pip3 install schedule
# 카메라 권한
sudo usermod -a -G video $USER
# 설치 확인
echo "=== 설치 확인 ==="
python3 -c "import cv2; print('✅ OpenCV:', cv2.__version__)"
python3 -c "import schedule; print('✅ Schedule: OK')"
python3 -c "import smtplib; print('✅ Email: OK')"
echo ""
echo "✅ 설치 완료! 재부팅 후 사용하세요."
echo "재부팅: sudo reboot"
EOF

# 실행 권한 부여
chmod +x install.sh

# 설치 실행
./install.sh
```

### 2️⃣ 재부팅

```bash
sudo reboot
```

### 3️⃣ 메인 프로그램 생성

재부팅 후:

```bash
cat > plant_monitor.py << 'EOF'
#!/usr/bin/env python3
import cv2
import time
import schedule
import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email.mime.text import MIMEText
from email import encoders
from datetime import datetime

# 이메일 설정
EMAIL = 'jmerrier0910@gmail.com'
PASSWORD = 'smvrcqoizxbxmyhy'

# 촬영 시간
TIMES = ["05:00", "12:00", "20:50"]

def take_and_send():
    """사진 찍고 바로 이메일 보내기"""
    print(f"📸 {datetime.now().strftime('%H:%M:%S')} 사진 촬영 시작...")
    
    # 1. 사진 촬영
    cap = cv2.VideoCapture(0)
    time.sleep(1)
    ret, frame = cap.read()
    cap.release()
    
    if not ret:
        print("❌ 촬영 실패")
        return
    
    # 2. 사진 저장
    filename = datetime.now().strftime("%Y%m%d_%H%M%S.jpg")
    cv2.imwrite(filename, frame)
    print(f"✅ 저장: {filename}")
    
    # 3. 이메일 전송
    try:
        msg = MIMEMultipart()
        msg['From'] = EMAIL
        msg['To'] = EMAIL
        msg['Subject'] = f"🌱 식물사진 {datetime.now().strftime('%m/%d %H:%M')}"
        
        msg.attach(MIMEText("식물 사진입니다 🌿", 'plain'))
        
        # 사진 첨부
        with open(filename, 'rb') as f:
            part = MIMEBase('application', 'octet-stream')
            part.set_payload(f.read())
        encoders.encode_base64(part)
        part.add_header('Content-Disposition', f'attachment; filename={filename}')
        msg.attach(part)
        
        # 전송
        server = smtplib.SMTP_SSL('smtp.gmail.com', 465)
        server.login(EMAIL, PASSWORD)
        server.send_message(msg)
        server.quit()
        
        print("✅ 이메일 전송 완료!")
        
    except Exception as e:
        print(f"❌ 전송 실패: {e}")

if __name__ == "__main__":
    print("🌱 식물 모니터링 시작")
    print(f"📅 촬영 시간: {', '.join(TIMES)}")
    
    # 스케줄 등록
    for t in TIMES:
        schedule.every().day.at(t).do(take_and_send)
        print(f"⏰ {t} 등록")
    
    print("🚀 실행 중... (Ctrl+C 종료)\n")
    
    while True:
        schedule.run_pending()
        time.sleep(60)
EOF

chmod +x plant_monitor.py
```

### 4️⃣ 테스트 (즉시 촬영/전송)

```bash
cat > test_now.py << 'EOF'
#!/usr/bin/env python3
import cv2
import smtplib
import time
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email.mime.text import MIMEText
from email import encoders
from datetime import datetime

print("📸 테스트 촬영 시작...")

cap = cv2.VideoCapture(0)
time.sleep(1)
ret, frame = cap.read()
cap.release()

if not ret:
    print("❌ 카메라 오류")
    exit(1)

filename = 'test.jpg'
cv2.imwrite(filename, frame)
print('✅ 사진 저장')

msg = MIMEMultipart()
msg['From'] = 'jmerrier0910@gmail.com'
msg['To'] = 'jmerrier0910@gmail.com'
msg['Subject'] = '🌱 테스트'
msg.attach(MIMEText('테스트입니다', 'plain'))

with open(filename, 'rb') as f:
    part = MIMEBase('application', 'octet-stream')
    part.set_payload(f.read())
encoders.encode_base64(part)
part.add_header('Content-Disposition', f'attachment; filename={filename}')
msg.attach(part)

server = smtplib.SMTP_SSL('smtp.gmail.com', 465)
server.login('jmerrier0910@gmail.com', 'smvrcqoizxbxmyhy')
server.send_message(msg)
server.quit()
print('✅ 이메일 전송!')
EOF

chmod +x test_now.py
python3 test_now.py
```

### 5️⃣ 메인 프로그램 실행

```bash
# 포그라운드 실행 (로그 보면서)
python3 plant_monitor.py

# 또는 백그라운드 실행
nohup python3 plant_monitor.py > plant.log 2>&1 &

# 백그라운드 실행 확인
ps aux | grep plant_monitor

# 로그 확인
tail -f plant.log
```

### 6️⃣ 자동 시작 설정 (부팅시 자동 실행)

```bash
# systemd 서비스 생성
sudo nano /etc/systemd/system/plant-monitor.service
```

내용:
```ini
[Unit]
Description=Plant Monitoring Service
After=network.target

[Service]
Type=simple
User=dli
WorkingDirectory=/home/dli
ExecStart=/usr/bin/python3 /home/dli/plant_monitor.py
Restart=always

[Install]
WantedBy=multi-user.target
```

활성화:
```bash
# 서비스 시작
sudo systemctl start plant-monitor

# 부팅시 자동 시작
sudo systemctl enable plant-monitor

# 상태 확인
sudo systemctl status plant-monitor

# 로그 확인
sudo journalctl -u plant-monitor -f
```

## 📌 요약

1. **설치**: `./install.sh` → 재부팅
2. **테스트**: `python3 test_now.py` (즉시 촬영/전송)
3. **실행**: `python3 plant_monitor.py`
4. **자동시작**: systemd 서비스 등록

## 🎯 완료!

- 매일 05:00, 12:00, 20:50에 자동 촬영
- 촬영 즉시 이메일 전송
- 부팅시 자동 시작 🚀
