#!/usr/bin/env python3
import cv2
import time
import schedule
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email import encoders
from datetime import datetime, timedelta
import logging
import os
import zipfile
from pathlib import Path

# 로깅 설정 (파일 크기 제한)
from logging.handlers import RotatingFileHandler

log_dir = Path('logs')
log_dir.mkdir(exist_ok=True)

# 로그 파일 최대 1MB, 최대 3개 파일 유지
file_handler = RotatingFileHandler(
    log_dir / 'plant_monitor.log', 
    maxBytes=1024*1024, 
    backupCount=3
)
console_handler = logging.StreamHandler()

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[file_handler, console_handler]
)

# 이메일 설정
EMAIL_CONFIG = {
    'address': 'jmerrier0910@gmail.com',
    'password': '',  # 앱 비밀번호
    'smtp_server': 'smtp.gmail.com',
    'smtp_port': 465
}

# 촬영 스케줄 설정
PHOTO_SCHEDULE = [
    "05:00",  # 새벽 5시
    "12:00",  # 정오 12시  
    "20:50"   # 밤 8시 50분
]

def capture_photo():
    """USB 카메라로 사진 촬영 (성공 확인된 방식)"""
    logging.info("📸 사진 촬영 시작...")
    
    try:
        # V4L2 백엔드로 카메라 열기
        cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
        
        if not cap.isOpened():
            # 백업 방식들 시도
            for backend in [cv2.CAP_V4L, cv2.CAP_ANY]:
                cap = cv2.VideoCapture(0, backend)
                if cap.isOpened():
                    break
            else:
                logging.error("❌ 모든 백엔드에서 카메라 열기 실패")
                return False
        
        # 해상도 설정 (안전한 크기)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        
        # 카메라 안정화
        time.sleep(1)
        
        # 몇 개 프레임 건너뛰기
        for _ in range(3):
            cap.read()
        
        # 실제 사진 촬영
        ret, frame = cap.read()
        cap.release()
        
        if not ret or frame is None:
            logging.error("❌ 프레임 읽기 실패")
            return False
        
        # 현재 시간으로 폴더 및 파일명 생성
        current_time = datetime.now()
        date_folder = current_time.strftime("%Y%m%d")
        Path(date_folder).mkdir(exist_ok=True)
        
        filename = current_time.strftime("%Y%m%d_%H%M%S.jpg")
        filepath = Path(date_folder) / filename
        
        # 이미지 저장
        if cv2.imwrite(str(filepath), frame):
            file_size = filepath.stat().st_size / 1024
            logging.info(f"✅ 사진 저장 성공: {filepath} ({file_size:.1f}KB)")
            return True
        else:
            logging.error("❌ 이미지 저장 실패")
            return False
            
    except Exception as e:
        logging.error(f"❌ 사진 촬영 중 오류: {str(e)}")
        return False

def capture_with_retry(max_retries=3):
    """재시도 기능 포함 사진 촬영"""
    for attempt in range(max_retries):
        if capture_photo():
            return True
        
        logging.warning(f"⚠️ 촬영 시도 {attempt + 1}/{max_retries} 실패")
        if attempt < max_retries - 1:
            logging.info("🔄 5초 후 재시도...")
            time.sleep(5)
    
    logging.error("❌ 최대 재시도 횟수 초과")
    return False

def create_daily_zip(target_date):
    """지정된 날짜의 모든 사진을 ZIP으로 압축"""
    logging.info(f"📦 {target_date.strftime('%Y-%m-%d')} 사진들 ZIP 압축 시작...")
    
    try:
        date_folder = Path(target_date.strftime("%Y%m%d"))
        
        if not date_folder.exists():
            logging.warning(f"⚠️ {date_folder} 폴더가 존재하지 않습니다.")
            return None
        
        # 해당 날짜의 모든 JPG 파일 찾기
        photo_files = list(date_folder.glob("*.jpg"))
        
        if not photo_files:
            logging.warning(f"⚠️ {date_folder}에 사진 파일이 없습니다.")
            return None
        
        # ZIP 파일명 생성
        zip_filename = f"plant_photos_{target_date.strftime('%Y%m%d')}.zip"
        
        # ZIP 파일 생성
        with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for photo_file in photo_files:
                # ZIP 내부에서는 날짜폴더/파일명 구조 유지
                archive_name = f"{date_folder.name}/{photo_file.name}"
                zipf.write(photo_file, archive_name)
        
        zip_size = Path(zip_filename).stat().st_size / (1024 * 1024)  # MB
        logging.info(f"✅ ZIP 압축 완료: {zip_filename} ({len(photo_files)}장, {zip_size:.1f}MB)")
        
        return zip_filename
        
    except Exception as e:
        logging.error(f"❌ ZIP 압축 중 오류: {str(e)}")
        return None

def send_daily_email(zip_file_path, target_date):
    """일간 사진 ZIP 파일을 이메일로 전송"""
    logging.info("📧 일간 리포트 이메일 전송 시작...")
    
    try:
        # 이메일 메시지 구성
        message = MIMEMultipart()
        message["From"] = EMAIL_CONFIG['address']
        message["To"] = EMAIL_CONFIG['address']
        message["Subject"] = f"🌱 식물 모니터링 일간 리포트 - {target_date.strftime('%Y년 %m월 %d일')}"
        
        # ZIP 파일 정보
        zip_size = Path(zip_file_path).stat().st_size / (1024 * 1024)  # MB
        with zipfile.ZipFile(zip_file_path, 'r') as zipf:
            file_count = len(zipf.namelist())
        
        # 이메일 본문
        body = f"""안녕하세요! 🌱

{target_date.strftime('%Y년 %m월 %d일')} 식물 모니터링 리포트를 보내드립니다.

📊 촬영 정보:
• 촬영 날짜: {target_date.strftime('%Y년 %m월 %d일 (%A)')}
• 촬영 시간: {', '.join(PHOTO_SCHEDULE)}
• 총 사진 수: {file_count}장
• 파일 크기: {zip_size:.1f}MB

📎 첨부 파일에는 하루 동안 촬영된 모든 식물 사진이 포함되어 있습니다.
식물의 성장 과정을 확인해보세요! 🌿

---
자동 전송 시스템 (젯슨 나노 4GB)
생성 시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
        """
        
        message.attach(MIMEText(body, "plain"))
        
        # ZIP 파일 첨부
        with open(zip_file_path, "rb") as attachment:
            part = MIMEBase('application', 'octet-stream')
            part.set_payload(attachment.read())
        
        encoders.encode_base64(part)
        part.add_header(
            'Content-Disposition',
            f'attachment; filename= {os.path.basename(zip_file_path)}'
        )
        message.attach(part)
        
        # SMTP 서버 연결 및 전송
        with smtplib.SMTP_SSL(EMAIL_CONFIG['smtp_server'], EMAIL_CONFIG['smtp_port']) as server:
            server.login(EMAIL_CONFIG['address'], EMAIL_CONFIG['password'])
            server.send_message(message)
        
        logging.info("✅ 일간 리포트 이메일 전송 완료!")
        return True
        
    except Exception as e:
        logging.error(f"❌ 이메일 전송 중 오류: {str(e)}")
        return False

def send_daily_report():
    """어제 사진들을 ZIP으로 만들어서 이메일 전송"""
    logging.info("📋 일간 리포트 작업 시작...")
    
    # 어제 날짜 (오늘 오전에 어제 사진들을 전송)
    yesterday = datetime.now().date() - timedelta(days=1)
    
    # ZIP 파일 생성
    zip_file = create_daily_zip(yesterday)
    
    if zip_file:
        # 이메일 전송
        if send_daily_email(zip_file, yesterday):
            logging.info("🎉 일간 리포트 전송 성공!")
            
            # 전송 완료 후 ZIP 파일 삭제 (공간 절약)
            try:
                os.remove(zip_file)
                logging.info(f"🗑️ ZIP 파일 삭제: {zip_file}")
            except:
                logging.warning(f"⚠️ ZIP 파일 삭제 실패: {zip_file}")
                
        else:
            logging.error("❌ 일간 리포트 전송 실패!")
    else:
        logging.warning("⚠️ 전송할 사진이 없습니다.")

def cleanup_old_photos(days_to_keep=7):
    """오래된 사진 폴더 정리 (디스크 공간 절약)"""
    try:
        cutoff_date = datetime.now().date() - timedelta(days=days_to_keep)
        
        for item in Path('.').iterdir():
            if item.is_dir() and len(item.name) == 8 and item.name.isdigit():
                try:
                    folder_date = datetime.strptime(item.name, '%Y%m%d').date()
                    if folder_date < cutoff_date:
                        # 폴더와 내용 삭제
                        import shutil
                        shutil.rmtree(item)
                        logging.info(f"🗑️ 오래된 폴더 삭제: {item.name}")
                except:
                    continue
                    
    except Exception as e:
        logging.error(f"❌ 폴더 정리 중 오류: {str(e)}")

def system_status():
    """시스템 상태 로깅"""
    try:
        # 디스크 사용량
        import shutil
        total, used, free = shutil.disk_usage('/')
        disk_usage = (used / total) * 100
        
        logging.info(f"💾 디스크 사용률: {disk_usage:.1f}% (여유공간: {free//1024//1024//1024}GB)")
        
        # 사진 폴더 개수
        photo_folders = [d for d in Path('.').iterdir() 
                        if d.is_dir() and len(d.name) == 8 and d.name.isdigit()]
        logging.info(f"📁 사진 폴더 개수: {len(photo_folders)}개")
        
    except Exception as e:
        logging.error(f"❌ 시스템 상태 확인 오류: {str(e)}")

if __name__ == "__main__":
    logging.info("=" * 60)
    logging.info("🌱 식물 모니터링 시스템 시작")
    logging.info("=" * 60)
    logging.info(f"📅 촬영 스케줄: {', '.join(PHOTO_SCHEDULE)}")
    logging.info(f"📧 이메일 전송: 매일 오전 9시 (어제 사진들)")
    logging.info("=" * 60)
    
    # 사진 촬영 스케줄 등록
    for time_str in PHOTO_SCHEDULE:
        schedule.every().day.at(time_str).do(capture_with_retry)
        logging.info(f"⏰ 스케줄 등록: 매일 {time_str}")
    
    # 일간 리포트 전송 (매일 오전 9시)
    schedule.every().day.at("09:00").do(send_daily_report)
    logging.info("📧 스케줄 등록: 매일 09:00 - 일간 리포트 전송")
    
    # 시스템 상태 체크 (매일 오전 8시)
    schedule.every().day.at("08:00").do(system_status)
    logging.info("💻 스케줄 등록: 매일 08:00 - 시스템 상태 체크")
    
    # 오래된 사진 정리 (매주 일요일 오전 10시)
    schedule.every().sunday.at("10:00").do(lambda: cleanup_old_photos(7))
    logging.info("🗑️ 스케줄 등록: 매주 일요일 10:00 - 오래된 사진 정리")
    
    # 시작 시 시스템 상태 체크
    system_status()
    
    try:
        logging.info("🚀 스케줄러 실행 중... (Ctrl+C로 종료)")
        
        while True:
            schedule.run_pending()
            time.sleep(30)  # 30초마다 스케줄 체크
            
    except KeyboardInterrupt:
        logging.info("👋 사용자에 의해 프로그램이 중단되었습니다.")
    except Exception as e:
        logging.error(f"❌ 예상치 못한 오류: {str(e)}")
    finally:
        logging.info("🔚 프로그램 종료")
