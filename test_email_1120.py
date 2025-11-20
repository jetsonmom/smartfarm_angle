
import cv2, smtplib, time
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email.mime.text import MIMEText
from email import encoders
from datetime import datetime

cap = cv2.VideoCapture(0)
time.sleep(1)
ret, frame = cap.read()
cap.release()

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

