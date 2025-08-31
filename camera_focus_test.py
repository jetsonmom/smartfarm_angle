#!/usr/bin/env python3
import cv2
import time
from datetime import datetime
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')

def test_camera_settings():
    """다양한 카메라 설정으로 테스트"""
    
    settings_list = [
      
        {
            'name': '중간 해상도 + 최적화',
            'width': 1280, 'height': 720,
            'autofocus': 1, 'exposure': 1, 'sharpness': 133,
            'contrast': 77, 'saturation': 90
        },
     {
            'name': '중간 해상도 + 최적화',
            'width': 1280, 'height': 720,
            'autofocus': 1, 'exposure': 1, 'sharpness': 128,
            'contrast': 83, 'saturation': 80
        },
     {
            'name': '중간 해상도 + 최적화',
            'width': 1280, 'height': 720,
            'autofocus': 1, 'exposure': 1, 'sharpness': 133,
            'contrast': 87, 'saturation': 80
        }
    ]
    
    for i, settings in enumerate(settings_list, 1):
        print(f"\n{'='*50}")
        print(f"🔍 테스트 {i}: {settings['name']}")
        print(f"{'='*50}")
        
        try:
            # 카메라 열기
            cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
            
            if not cap.isOpened():
                for backend in [cv2.CAP_V4L, cv2.CAP_ANY]:
                    cap = cv2.VideoCapture(0, backend)
                    if cap.isOpened():
                        break
                else:
                    print("❌ 카메라 열기 실패")
                    continue
            
            # 설정 적용
            cap.set(cv2.CAP_PROP_FRAME_WIDTH, settings['width'])
            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, settings['height'])
            
            if 'autofocus' in settings:
                cap.set(cv2.CAP_PROP_AUTOFOCUS, settings['autofocus'])
            if 'exposure' in settings:
                cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, settings['exposure'])
            if 'sharpness' in settings:
                cap.set(cv2.CAP_PROP_SHARPNESS, settings['sharpness'])
            if 'contrast' in settings:
                cap.set(cv2.CAP_PROP_CONTRAST, settings['contrast'])
            if 'saturation' in settings:
                cap.set(cv2.CAP_PROP_SATURATION, settings['saturation'])
            
            # 초점 맞춤 대기
            print("⏳ 초점 맞추는 중... (5초)")
            time.sleep(5)
            
            # 프레임 안정화
            for _ in range(10):
                cap.read()
            
            # 사진 촬영
            ret, frame = cap.read()
            cap.release()
            
            if ret and frame is not None:
                timestamp = datetime.now().strftime("%H%M%S")
                filename = f"focus_test_{i}_{timestamp}.jpg"
                
                if cv2.imwrite(filename, frame):
                    file_size = len(cv2.imencode('.jpg', frame)[1]) / 1024
                    print(f"✅ 저장 성공: {filename} ({file_size:.1f}KB)")
                    print(f"📏 해상도: {frame.shape[1]}x{frame.shape[0]}")
                else:
                    print("❌ 저장 실패")
            else:
                print("❌ 촬영 실패")
                
        except Exception as e:
            print(f"❌ 오류: {e}")

def manual_focus_adjustment():
    """수동 초점 조정 테스트"""
    print(f"\n{'='*50}")
    print("🎯 수동 초점 조정 테스트")
    print(f"{'='*50}")
    
    try:
        cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
        
        if not cap.isOpened():
            print("❌ 카메라 열기 실패")
            return
        
        # 해상도 설정
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        
        # 수동 초점 모드
        cap.set(cv2.CAP_PROP_AUTOFOCUS, 0)  # 자동 초점 끄기
        
        # 다양한 초점 거리로 테스트
        focus_values = [0, 50, 100, 150, 200, 255]  # 0~255 범위
        
        for focus_val in focus_values:
            print(f"🔍 초점값 {focus_val} 테스트 중...")
            
            cap.set(cv2.CAP_PROP_FOCUS, focus_val)
            time.sleep(3)  # 초점 조정 대기
            
            # 프레임 안정화
            for _ in range(5):
                cap.read()
            
            ret, frame = cap.read()
            
            if ret and frame is not None:
                filename = f"manual_focus_{focus_val}.jpg"
                if cv2.imwrite(filename, frame):
                    print(f"✅ 저장: {filename}")
        
        cap.release()
        print("🎯 수동 초점 테스트 완료!")
        
    except Exception as e:
        print(f"❌ 오류: {e}")

def camera_info():
    """카메라 정보 확인"""
    print(f"\n{'='*50}")
    print("📋 카메라 정보")
    print(f"{'='*50}")
    
    try:
        cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
        
        if not cap.isOpened():
            print("❌ 카메라 열기 실패")
            return
        
        # 현재 설정값들 확인
        properties = {
            'FRAME_WIDTH': cv2.CAP_PROP_FRAME_WIDTH,
            'FRAME_HEIGHT': cv2.CAP_PROP_FRAME_HEIGHT,
            'FPS': cv2.CAP_PROP_FPS,
            'BRIGHTNESS': cv2.CAP_PROP_BRIGHTNESS,
            'CONTRAST': cv2.CAP_PROP_CONTRAST,
            'SATURATION': cv2.CAP_PROP_SATURATION,
            'SHARPNESS': cv2.CAP_PROP_SHARPNESS,
            'AUTOFOCUS': cv2.CAP_PROP_AUTOFOCUS,
            'FOCUS': cv2.CAP_PROP_FOCUS,
            'AUTO_EXPOSURE': cv2.CAP_PROP_AUTO_EXPOSURE
        }
        
        for name, prop in properties.items():
            try:
                value = cap.get(prop)
                print(f"{name}: {value}")
            except:
                print(f"{name}: 지원하지 않음")
        
        cap.release()
        
    except Exception as e:
        print(f"❌ 오류: {e}")

def main():
    print("🔧 카메라 초점 및 화질 개선 도구")
    print("="*60)
    
    while True:
        print("\n📋 메뉴:")
        print("1. 자동 설정 테스트 (3가지)")
        print("2. 수동 초점 조정 테스트")
        print("3. 카메라 정보 확인")
        print("4. 종료")
        
        choice = input("\n선택하세요 (1-4): ").strip()
        
        if choice == '1':
            test_camera_settings()
        elif choice == '2':
            manual_focus_adjustment()
        elif choice == '3':
            camera_info()
        elif choice == '4':
            print("👋 종료합니다.")
            break
        else:
            print("❌ 잘못된 선택입니다.")

if __name__ == "__main__":
    main()
