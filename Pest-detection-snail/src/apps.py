from flask import Flask, render_template, Response
import cv2
import urllib.request
import numpy as np
from ultralytics import YOLO
import os
import requests
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app)

# Load the YOLO model
model = YOLO(f"{os.getcwd()}/snail.pt")

# IP address of ESP32-CAM
url = "http://192.168.100.9/stream"
esp_url = "http://192.168.100.9/toggle-relay"  # Endpoint to toggle relay on ESP32

def generate_frames():
    bytes_stream = b""
    while True:
        try:
            with urllib.request.urlopen(url) as stream:
                while True:
                    bytes_stream += stream.read(1024)
                    a = bytes_stream.find(b"\xff\xd8")  # Start of JPEG
                    b = bytes_stream.find(b"\xff\xd9")  # End of JPEG

                    if a != -1 and b != -1:
                        jpg = bytes_stream[a:b + 2]
                        bytes_stream = bytes_stream[b + 2:]

                        img_np = np.frombuffer(jpg, dtype=np.uint8)
                        img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)

                        if img is not None:
                            # Fire detection
                            results = model(img, conf=0.8, imgsz=(448))
                            detected = False
                            # Draw bounding boxes
                            for result in results:
                                boxes = result.boxes
                                for box in boxes:
                                    x1, y1, x2, y2 = box.xyxy.tolist()[0]
                                    c = box.cls
                                    x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
                                    label = model.names[int(c)]
                                    cv2.rectangle(img, (x1, y1), (x2, y2), (0, 0, 255), 2)
                                    cv2.putText(
                                        img,
                                        label,
                                        (x1, y1 - 10),
                                        cv2.FONT_HERSHEY_SIMPLEX,
                                        0.9,
                                        (0, 255, 0),
                                        2,
                                    )
                                    detected = True

                            # if detected:
                            #     try:
                            #         # Send request to ESP32 to toggle relay
                            #         requests.get(esp_url)
                            #         socketio.emit('toggle_relay', {'status': 'Relay toggled'})
                            #     except Exception as e:
                            #         print(f"Failed to send request to ESP32: {e}")
                            
                            if detected:
                                    try:
                                        # Send request to ESP32 to toggle relay
                                        requests.get(esp_url)
                                        # Emit a refresh event to the client
                                        socketio.emit('refresh_stream', {'status': 'Object detected'})
                                    except Exception as e:
                                        print(f"Failed to send request to ESP32: {e}")


                            # Encode image as JPEG
                            ret, buffer = cv2.imencode('.jpg', img)
                            frame = buffer.tobytes()

                            yield (b'--frame\r\n'
                                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

        except urllib.error.HTTPError as e:
            print(f"HTTP Error: {e.code} - {e.reason}")
        except Exception as e:
            print(f"Error: {e}")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

# @socketio.on('toggle_relay')
# def handle_toggle_relay(data):
#     print("WebSocket message received:", data)

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)
