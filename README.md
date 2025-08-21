# Sani Flush Camera Server - Docker Setup

This Docker setup simulates your ESP32-CAM functionality for development and testing.

## Quick Start

1. **Build and run the containers:**
   ```bash
   docker-compose up --build
   ```

2. **Access the cameras:**
   - Left Camera: http://localhost:8080
   - Right Camera: http://localhost:8081
   - Main Interface: http://localhost

## Features

- **Camera Simulation**: Generates test images with timestamps
- **Settings API**: Adjust camera parameters like brightness, contrast, etc.
- **Live Stream**: MJPEG streaming endpoint
- **Health Checks**: Container health monitoring
- **Reverse Proxy**: Nginx routing for DuckDNS-like functionality

## API Endpoints

- `GET /` - Main camera interface
- `GET /capture` - Capture single image (JPEG)
- `GET /stream` - Live stream page
- `GET /stream_raw` - Raw MJPEG stream
- `GET /settings` - Camera settings interface
- `GET /api/settings?setting=<name>&action=<+/-/toggle>` - Adjust settings
- `GET /api/values` - Get current settings
- `GET /set_bw?mode=<color/bw>` - Set color mode

## Integration with Main Controller

Update your main controller code to use these endpoints:

```cpp
// Replace DuckDNS URLs with Docker container URLs
const char* leftCameraURL = "http://localhost:8080/capture";
const char* rightCameraURL = "http://localhost:8081/capture";
```

## Production Deployment

For production, consider:
- Using real camera hardware or USB cameras
- Setting up proper SSL certificates
- Configuring actual DuckDNS domains
- Adding authentication/security
- Using persistent storage for logs

## Stopping the Services

```bash
docker-compose down
```