# Local Web UI Testing Guide

How to test Angular web UI changes locally without flashing to ESP32.

## Quick Start

```bash
cd main/http_server/axe-os

# Install dependencies (first time only)
npm install

# Start development server
npm start

# Open browser to http://localhost:4200
```

## Setting Up API Proxy

To connect your local web UI to the ESP32's API:

### Step 1: Find Your ESP32 IP

```bash
# From serial monitor, note the IP address
# Example: 192.168.1.65
```

### Step 2: Create Proxy Configuration

Create `main/http_server/axe-os/proxy.conf.json`:

```json
{
  "/api": {
    "target": "http://192.168.1.65",
    "secure": false,
    "logLevel": "debug",
    "changeOrigin": true
  },
  "/ws": {
    "target": "ws://192.168.1.65",
    "secure": false,
    "ws": true,
    "logLevel": "debug"
  }
}
```

### Step 3: Update package.json

Edit `main/http_server/axe-os/package.json`:

```json
{
  "scripts": {
    "start": "ng serve --proxy-config proxy.conf.json",
    "build": "ng build --configuration production"
  }
}
```

### Step 4: Start with Proxy

```bash
npm start
```

Now `http://localhost:4200` will forward API calls to your ESP32!

## Development Workflow

```bash
# 1. Make changes to TypeScript/HTML/SCSS files
# 2. Save (auto-reloads in browser)
# 3. Test functionality
# 4. When satisfied, build for production:
npm run build

# 5. Flash to ESP32
cd ../../..
./build.sh -f
```

## Hot Reload

Changes to these files auto-reload:
- `src/**/*.ts` (TypeScript)
- `src/**/*.html` (Templates)
- `src/**/*.scss` (Styles)

No need to rebuild or refresh!

## Debugging

**Browser Console**: Press F12
- See console logs
- Network tab shows API calls
- Elements tab for styling

**Angular DevTools**: Install Chrome extension
- Component inspector
- Performance profiling
