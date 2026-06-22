# Beout_OS - Admin Dashboard

This is the React + TypeScript frontend dashboard for the Beout_OS Security Appliance.

## Features
- **Modern Premium Design**: Utilizes glassmorphism, responsive grid layouts, and smooth micro-animations.
- **Secure Authentication**: Connects directly to the `/api/auth/login` endpoint of the `api` daemon.
- **System Monitoring**: Displays appliance health and real-time interface states natively.

## Development
Run the Vite development server with hot-module reloading:
```bash
npm install
npm run dev
```

During development, requests to `/api` are automatically proxied to the native C++ API running locally on port `8443`.

## Build
```bash
npm run build
```
This produces optimized static assets in the `dist/` directory, which can be served directly by the appliance.
