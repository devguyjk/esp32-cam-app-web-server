#!/usr/bin/env python3
"""
Camera Server - Docker-compatible version of ESP32-CAM functionality
Simulates the ESP32-CAM web server for development/testing
"""

from flask import Flask, Response, request, jsonify, render_template_string
import cv2
import numpy as np
from PIL import Image
import io
import time
import hashlib
import os

app = Flask(__name__)

# Simulate camera settings
camera_settings = {
    'brightness': 0,
    'contrast': 0,
    'saturation': 0,
    'sharpness': 2,
    'ae_level': 0,
    'gainceiling': 4,
    'wb_mode': 0,
    'denoise': 5,
    'aec': True,
    'agc': True,
    'awb': True,
    'awb_gain': True,
    'lenc': True,
    'wpc': True,
    'bpc': True,
    'raw_gma': True,
    'special_effect': 0  # 0=normal, 2=grayscale
}

# Activity log
activity_log = []

def add_log(message):
    timestamp = int(time.time() * 1000)
    activity_log.append(f"{timestamp}: {message}")
    if len(activity_log) > 300:
        activity_log.pop(0)

def generate_unique_id():
    """Generate a unique 8-character ID similar to ESP32 chip ID"""
    hostname = os.environ.get('HOSTNAME', 'docker-cam')
    return hashlib.md5(hostname.encode()).hexdigest()[:8]

def generate_test_image():
    """Generate a test camera image"""
    # Create a test image with timestamp
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    img[:] = (50, 100, 150)  # Blue background
    
    # Add timestamp text
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    cv2.putText(img, f"Camera Feed", (50, 100), cv2.FONT_HERSHEY_SIMPLEX, 2, (255, 255, 255), 3)
    cv2.putText(img, timestamp, (50, 200), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
    cv2.putText(img, f"ID: {generate_unique_id()}", (50, 250), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
    
    # Apply camera settings
    if camera_settings['special_effect'] == 2:  # Grayscale
        img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)
    
    # Apply brightness/contrast
    img = cv2.convertScaleAbs(img, alpha=1 + camera_settings['contrast']*0.1, 
                             beta=camera_settings['brightness']*10)
    
    return img

@app.route('/')
def root():
    """Main camera interface"""
    html = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Docker Camera Server</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
            img { max-width: 90%; border: 2px solid #007bff; border-radius: 8px; }
            .controls { margin: 20px 0; }
            button { padding: 10px 20px; margin: 5px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; }
            button:hover { background: #0056b3; }
        </style>
    </head>
    <body>
        <h1>Docker Camera Server</h1>
        <p>Camera ID: """ + generate_unique_id() + """</p>
        <div class="controls">
            <button onclick="refreshImage()">Capture Image</button>
            <button onclick="window.open('/stream', '_blank')">Live Stream</button>
            <button onclick="window.open('/settings', '_blank')">Settings</button>
        </div>
        <img id="camera" src="/capture" alt="Camera Feed">
        
        <script>
            function refreshImage() {
                document.getElementById('camera').src = '/capture?t=' + Date.now();
            }
            setInterval(refreshImage, 3000); // Auto refresh every 3 seconds
        </script>
    </body>
    </html>
    """
    return html

@app.route('/capture')
def capture():
    """Capture endpoint - returns JPEG image"""
    add_log("Handling capture endpoint")
    
    use_flash = request.args.get('flash') == 'true'
    if use_flash:
        add_log("Flash activated")
    
    # Generate test image
    img = generate_test_image()
    
    # Convert to JPEG
    _, buffer = cv2.imencode('.jpg', img, [cv2.IMWRITE_JPEG_QUALITY, 85])
    
    add_log(f"Snapshot sent ({len(buffer)} bytes)")
    
    return Response(buffer.tobytes(), mimetype='image/jpeg')

@app.route('/stream')
def stream_page():
    """Stream page"""
    html = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Camera Stream</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }
            img { max-width: 90%; border: 2px solid #007bff; border-radius: 8px; }
        </style>
    </head>
    <body>
        <h1>Live Camera Stream</h1>
        <img src="/stream_raw" alt="Live Stream">
    </body>
    </html>
    """
    return html

@app.route('/stream_raw')
def stream_raw():
    """Raw MJPEG stream"""
    def generate():
        while True:
            img = generate_test_image()
            _, buffer = cv2.imencode('.jpg', img, [cv2.IMWRITE_JPEG_QUALITY, 85])
            
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n'
                   b'Content-Length: ' + str(len(buffer)).encode() + b'\r\n\r\n' +
                   buffer.tobytes() + b'\r\n')
            time.sleep(0.1)  # 10 FPS
    
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/settings')
def settings():
    """Camera settings page"""
    html = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Camera Settings</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; }
            .setting { margin: 10px 0; display: flex; justify-content: space-between; align-items: center; }
            button { padding: 5px 10px; margin: 0 5px; }
            .value { font-weight: bold; min-width: 50px; text-align: center; }
        </style>
    </head>
    <body>
        <h1>Camera Settings</h1>
        <div id="settings"></div>
        
        <script>
            function loadSettings() {
                fetch('/api/values')
                    .then(r => r.json())
                    .then(data => {
                        let html = '';
                        Object.keys(data).forEach(key => {
                            if (typeof data[key] === 'boolean') {
                                html += `<div class="setting">
                                    <span>${key}</span>
                                    <div>
                                        <span class="value">${data[key] ? 'ON' : 'OFF'}</span>
                                        <button onclick="toggleSetting('${key}')">Toggle</button>
                                    </div>
                                </div>`;
                            } else {
                                html += `<div class="setting">
                                    <span>${key}</span>
                                    <div>
                                        <button onclick="adjustSetting('${key}', '-')">-</button>
                                        <span class="value">${data[key]}</span>
                                        <button onclick="adjustSetting('${key}', '+')">+</button>
                                    </div>
                                </div>`;
                            }
                        });
                        document.getElementById('settings').innerHTML = html;
                    });
            }
            
            function adjustSetting(setting, action) {
                fetch(`/api/settings?setting=${setting}&action=${action}`)
                    .then(() => loadSettings());
            }
            
            function toggleSetting(setting) {
                fetch(`/api/settings?setting=${setting}&action=toggle`)
                    .then(() => loadSettings());
            }
            
            loadSettings();
        </script>
    </body>
    </html>
    """
    return html

@app.route('/api/settings')
def api_settings():
    """API endpoint for camera settings"""
    setting = request.args.get('setting')
    action = request.args.get('action')
    
    if not setting or not action:
        return "Missing parameters", 400
    
    if action == 'toggle':
        if setting in camera_settings and isinstance(camera_settings[setting], bool):
            camera_settings[setting] = not camera_settings[setting]
            return f"{setting} {'enabled' if camera_settings[setting] else 'disabled'}"
    
    elif action in ['+', '-']:
        if setting in camera_settings and isinstance(camera_settings[setting], int):
            delta = 1 if action == '+' else -1
            camera_settings[setting] = max(-2, min(8, camera_settings[setting] + delta))
            return f"{setting} set to {camera_settings[setting]}"
    
    return "Setting applied", 200

@app.route('/api/values')
def api_values():
    """Get current camera settings"""
    return jsonify(camera_settings)

@app.route('/api/log')
def api_log():
    """Get activity log"""
    return '<br>'.join(activity_log)

@app.route('/set_bw')
def set_bw():
    """Set black and white mode"""
    mode = request.args.get('mode', 'color')
    camera_settings['special_effect'] = 2 if mode == 'bw' else 0
    return f"Mode set to {mode}"

if __name__ == '__main__':
    add_log("Camera server starting")
    app.run(host='0.0.0.0', port=80, debug=False)