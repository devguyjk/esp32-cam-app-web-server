#ifndef TEMPLATES_H
#define TEMPLATES_H

String getCommonHeader(String uniqueId, String ipAddress) {
  String header = R"rawliteral(
<style>
.header{background:#007bff;color:white;padding:15px 20px;margin-bottom:20px;position:relative}
.header h1{margin:0;font-size:24px}
.nav{font-size:14px;margin-top:8px}
.nav a{color:#cce7ff;text-decoration:none;margin-right:15px}
.nav a:hover{color:white;text-decoration:underline}
.wifi-status{position:absolute;top:15px;right:20px;background:rgba(255,255,255,0.1);padding:8px;border-radius:5px;font-size:11px;border:1px solid rgba(255,255,255,0.3);display:flex;align-items:center;gap:8px}
.wifi-bars{display:flex;gap:1px;align-items:end}
.bar{width:4px;height:12px;background:rgba(255,255,255,0.3)}
.bar.active{background:var(--bar-color)}
</style>
<div class="header">
<h1>Sani Flush Cam )rawliteral" + uniqueId + R"rawliteral( - )rawliteral" + ipAddress + R"rawliteral(</h1>
<div class="nav">
<a href="/">Home</a> | <a href="/settings">Settings</a> | <a href="/stream" target="_blank">Stream</a> | <a href="/capture" target="_blank">Capture</a>
</div>
<div class="wifi-status">
<div id="wifiText">WiFi: Loading...</div>
<div class="wifi-bars" id="wifiBars">
<div class="bar" style="height:3px"></div>
<div class="bar" style="height:5px"></div>
<div class="bar" style="height:7px"></div>
<div class="bar" style="height:9px"></div>
<div class="bar" style="height:11px"></div>
<div class="bar" style="height:13px"></div>
<div class="bar" style="height:15px"></div>
<div class="bar" style="height:17px"></div>
</div>
</div>
</div>
<script>
function updateWifiStatus(){fetch('/api/wifi').then(r=>r.json()).then(d=>{const rssi=d.rssi;let bars=0,color='#ff0000',text='Poor';if(rssi>=-50){bars=8;color='#00ff00';text='Excellent'}else if(rssi>=-60){bars=6;color='#80ff00';text='Good'}else if(rssi>=-70){bars=4;color='#ffff00';text='Fair'}else if(rssi>=-80){bars=2;color='#ff8000';text='Weak'}else{bars=1;color='#ff0000';text='Poor'}document.getElementById('wifiText').innerHTML=d.ssid+'<br>'+text+' ('+rssi+'dBm)';const barElements=document.querySelectorAll('#wifiBars .bar');barElements.forEach((bar,i)=>{if(i<bars){bar.classList.add('active');bar.style.setProperty('--bar-color',color)}else{bar.classList.remove('active')}})}).catch(e=>{console.error('WiFi error:',e);document.getElementById('wifiText').innerHTML='WiFi: Error'})}setInterval(updateWifiStatus,10000);updateWifiStatus();
</script>
)rawliteral";
  return header;
}

const char* rootPageTemplate = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>TITLE_PLACEHOLDER</title>
</head>
<body>
HEADER_PLACEHOLDER
<div style="display:flex;gap:20px;margin:20px">
<div style="flex:1">
<h2>Camera Preview</h2>
<img id="preview" src="/capture" style="max-width:100%;border:2px solid #007bff;border-radius:8px">
<br><br>
<button onclick="refreshPreview()">Refresh Preview</button>
<h2>Available Endpoints:</h2>
<ul>
<li><strong><a href="/">GET /</a></strong> - This API reference page</li>
<li><strong><a href="/capture">GET /capture</a></strong> - Capture image without flash</li>
<li><strong><a href="/capture?flash=true">GET /capture?flash=true</a></strong> - Capture image with flash</li>
<li><strong><a href="/stream" target="_blank">GET /stream</a></strong> - Live video stream (MJPEG)</li>
<li><strong><a href="/settings">GET /settings</a></strong> - Camera settings control panel</li>
<li><strong>GET /api/settings?setting=brightness&action=+</strong> - Adjust camera settings</li>
<li><strong><a href="/set_bw?mode=bw">GET /set_bw?mode=bw</a></strong> - Set black & white mode</li>
<li><strong><a href="/set_bw?mode=color">GET /set_bw?mode=color</a></strong> - Set color mode</li>
<li><strong><a href="/api/values">GET /api/values</a></strong> - Get current camera settings (JSON)</li>
</ul>
</div>
<div style="flex:1">
<h2>Activity Log (Last 300 entries)</h2>
<div id="log" style="height:400px;overflow-y:scroll;border:1px solid #ccc;padding:10px;background:#f8f9fa;font-family:monospace;font-size:12px"></div>
<button onclick="refreshLog()">Refresh Log</button>
</div>
</div>
<script>
function refreshPreview(){document.getElementById('preview').src='/capture?'+Date.now()}
function refreshLog(){fetch('/api/log').then(r=>r.text()).then(d=>document.getElementById('log').innerHTML=d)}
setInterval(refreshLog,5000)
refreshLog()
</script>
</body>
</html>
)rawliteral";

const char* streamPageTemplate = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>TITLE_PLACEHOLDER</title>
</head>
<body>
HEADER_PLACEHOLDER
<div style="text-align:center;padding:20px">
<h2>Live Video Stream</h2>
<img src="/stream_raw" style="max-width:90%;border:2px solid #007bff;border-radius:8px">
</div>
</body>
</html>
)rawliteral";

const char* settingsPageTemplate = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>TITLE_PLACEHOLDER</title>
<style>
body { font-family: Arial, sans-serif; margin: 0; background: #f5f5f5; }
.container { max-width: 1200px; margin: 0 auto; padding: 20px; }
.video-container { text-align: center; margin-bottom: 30px; }
.settings-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
.setting-group { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.setting-group h3 { margin-top: 0; color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }
.setting-item { display: flex; justify-content: space-between; align-items: center; margin: 15px 0; }
.setting-controls { display: flex; gap: 10px; align-items: center; }
button { padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
.btn-dec { background: #dc3545; color: white; }
.btn-inc { background: #28a745; color: white; }
.btn-toggle { background: #007bff; color: white; }
button:hover { opacity: 0.8; }
.value { font-weight: bold; min-width: 30px; text-align: center; }
.mode-controls { text-align: center; margin: 20px 0; }
.mode-controls label { margin: 0 15px; }
</style>
</head>
<body>
HEADER_PLACEHOLDER
<div class="container">
<div class="video-container">
<img id="preview" src="/capture" style="width:90%; max-width: 640px; border:2px solid #007bff; border-radius: 8px;">
<br><br>
<label><input type="radio" name="refresh" value="manual" checked onchange="setRefresh(this.value)"> Manual Refresh</label>
<label><input type="radio" name="refresh" value="auto" onchange="setRefresh(this.value)"> Auto Refresh (3s)</label>
<label><input type="checkbox" id="useFlash" checked> Use Flash</label>
<button onclick="refreshPreview()">Refresh Now</button>
</div>
<div class="mode-controls">
<label><input type="radio" name="mode" value="color" onchange="setMode(this.value)" checked> Color</label>
<label><input type="radio" name="mode" value="bw" onchange="setMode(this.value)"> Black & White</label>
</div>
<div class="settings-grid">
<div class="setting-group">
<h3>Exposure & Light</h3>
<div class="setting-item">
<span>Auto Exposure (AEC)</span>
<div class="setting-controls">
<span class="value" id="aec">ON</span>
<button class="btn-toggle" onclick="toggleSetting('aec')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>AE Level</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('ae_level', '-')">-</button>
<span class="value" id="ae_level">0</span>
<button class="btn-inc" onclick="adjustSetting('ae_level', '+')">+</button>
</div>
</div>
<div class="setting-item">
<span>Auto Gain (AGC)</span>
<div class="setting-controls">
<span class="value" id="agc">ON</span>
<button class="btn-toggle" onclick="toggleSetting('agc')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>Gain Ceiling</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('gainceiling', '-')">-</button>
<span class="value" id="gainceiling">4</span>
<button class="btn-inc" onclick="adjustSetting('gainceiling', '+')">+</button>
</div>
</div>
</div>
<div class="setting-group">
<h3>Image Quality</h3>
<div class="setting-item">
<span>Brightness</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('brightness', '-')">-</button>
<span class="value" id="brightness">0</span>
<button class="btn-inc" onclick="adjustSetting('brightness', '+')">+</button>
</div>
</div>
<div class="setting-item">
<span>Contrast</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('contrast', '-')">-</button>
<span class="value" id="contrast">0</span>
<button class="btn-inc" onclick="adjustSetting('contrast', '+')">+</button>
</div>
</div>
<div class="setting-item">
<span>Saturation</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('saturation', '-')">-</button>
<span class="value" id="saturation">0</span>
<button class="btn-inc" onclick="adjustSetting('saturation', '+')">+</button>
</div>
</div>
<div class="setting-item">
<span>Sharpness</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('sharpness', '-')">-</button>
<span class="value" id="sharpness">0</span>
<button class="btn-inc" onclick="adjustSetting('sharpness', '+')">+</button>
</div>
</div>
</div>
<div class="setting-group">
<h3>White Balance</h3>
<div class="setting-item">
<span>Auto White Balance</span>
<div class="setting-controls">
<span class="value" id="awb">ON</span>
<button class="btn-toggle" onclick="toggleSetting('awb')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>WB Mode</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('wb_mode', '-')">-</button>
<span class="value" id="wb_mode">0</span>
<button class="btn-inc" onclick="adjustSetting('wb_mode', '+')">+</button>
</div>
</div>
<div class="setting-item">
<span>AWB Gain</span>
<div class="setting-controls">
<span class="value" id="awb_gain">ON</span>
<button class="btn-toggle" onclick="toggleSetting('awb_gain')">Toggle</button>
</div>
</div>
</div>
<div class="setting-group">
<h3>Corrections</h3>
<div class="setting-item">
<span>Lens Correction</span>
<div class="setting-controls">
<span class="value" id="lenc">ON</span>
<button class="btn-toggle" onclick="toggleSetting('lenc')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>White Pixel Correction</span>
<div class="setting-controls">
<span class="value" id="wpc">ON</span>
<button class="btn-toggle" onclick="toggleSetting('wpc')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>Black Pixel Correction</span>
<div class="setting-controls">
<span class="value" id="bpc">ON</span>
<button class="btn-toggle" onclick="toggleSetting('bpc')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>Gamma Correction</span>
<div class="setting-controls">
<span class="value" id="raw_gma">ON</span>
<button class="btn-toggle" onclick="toggleSetting('raw_gma')">Toggle</button>
</div>
</div>
<div class="setting-item">
<span>Denoise</span>
<div class="setting-controls">
<button class="btn-dec" onclick="adjustSetting('denoise', '-')">-</button>
<span class="value" id="denoise">0</span>
<button class="btn-inc" onclick="adjustSetting('denoise', '+')">+</button>
</div>
</div>
</div>
</div>
</div>
<script>
function setMode(mode) {
fetch("/set_bw?mode=" + mode)
.then(resp => resp.text())
.then(data => console.log(data))
.catch(err => console.error("Error setting mode:", err));
}
let autoRefreshInterval;
function adjustSetting(setting, action) {
fetch(`/api/settings?setting=${setting}&action=${action}`)
.then(resp => resp.text())
.then(data => {
console.log(data);
loadCurrentValues();
setTimeout(refreshPreview, 500);
})
.catch(err => console.error("Error adjusting " + setting, err));
}
function toggleSetting(setting) {
fetch(`/api/settings?setting=${setting}&action=toggle`)
.then(resp => resp.text())
.then(data => {
console.log(data);
loadCurrentValues();
setTimeout(refreshPreview, 500);
})
.catch(err => console.error("Error toggling " + setting, err));
}
function refreshPreview() {
const useFlash = document.getElementById('useFlash').checked;
const flashParam = useFlash ? '&flash=true' : '';
document.getElementById('preview').src = '/capture?t=' + Date.now() + flashParam;
}
function setRefresh(mode) {
if (autoRefreshInterval) clearInterval(autoRefreshInterval);
if (mode === 'auto') {
autoRefreshInterval = setInterval(refreshPreview, 3000);
}
}
function loadCurrentValues() {
fetch('/api/values')
.then(resp => resp.json())
.then(data => {
Object.keys(data).forEach(key => {
const element = document.getElementById(key);
if (element) {
if (typeof data[key] === 'boolean') {
element.textContent = data[key] ? 'ON' : 'OFF';
element.style.color = data[key] ? '#28a745' : '#dc3545';
} else {
element.textContent = data[key];
}
}
});
})
.catch(err => console.error('Error loading values:', err));
}
window.onload = () => {
loadCurrentValues();
};
</script>
</body>
</html>
)rawliteral";

#endif