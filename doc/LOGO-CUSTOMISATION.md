# Logo and Header Customisation Guide

Guide for customising the ESP-Miner AxeOS interface with Cuneiform branding, logos, and headers.

## Overview

The ESP-Miner UI has several places where branding appears:
1. **Topbar/Header** - Main navigation at top of page
2. **Footer** - Bottom of each page
3. **Favicon** - Browser tab icon
4. **Page Title** - Browser tab text
5. **Menu** - Side menu (if applicable)

---

## Part 1: Adding Your Logo

### Step 1: Prepare Your Logo

**Logo Specifications**:
- **Format**: SVG (preferred) or PNG with transparent background
- **Size**: 150-200px wide, 40-50px tall (approximate)
- **Colour**: Should work on both light and dark backgrounds
- **File size**: Keep under 50KB for fast loading

### Step 2: Add Logo to Assets

```bash
cd ~/Work/ESP-Miner/main/http_server/axe-os

# Copy your logo
cp /path/to/cuneiform-logo.svg src/assets/logo-cuneiform.svg
# Or if PNG:
cp /path/to/cuneiform-logo.png src/assets/logo-cuneiform.png
```

---

## Part 2: Customising the Topbar (Header)

### Find the Topbar Component

```bash
cd ~/Work/ESP-Miner/main/http_server/axe-os

# List topbar files
ls -la src/app/layout/app.topbar.component.*
```

### Edit Topbar HTML

```bash
nano src/app/layout/app.topbar.component.html
```

**Look for the existing content** (around line 1-20) and modify it.

### Example: Add Logo to Topbar

```html
<div class="layout-topbar">
    <div class="layout-topbar-logo-container">
        <!-- Add Cuneiform Logo -->
        <img src="assets/logo-cuneiform.svg" 
             alt="Cuneiform" 
             class="layout-topbar-logo"
             height="40">
        
        <!-- Or conditionally show based on theme -->
        <img *ngIf="currentTheme === 'cuneiform'" 
             src="assets/logo-cuneiform.svg" 
             alt="Cuneiform" 
             class="layout-topbar-logo"
             height="40">
        
        <img *ngIf="currentTheme !== 'cuneiform'" 
             src="assets/logo-default.svg" 
             alt="Miner" 
             class="layout-topbar-logo"
             height="40">
    </div>
    
    <!-- Rest of topbar content -->
    <div class="layout-topbar-menu-container">
        <!-- Navigation items, etc. -->
    </div>
</div>
```

### Add Styling to Topbar

```bash
nano src/app/layout/app.topbar.component.ts
```

If you need to add theme detection logic:

```typescript
import { Component } from '@angular/core';
import { ThemeService } from '../services/theme.service';

@Component({
  selector: 'app-topbar',
  templateUrl: './app.topbar.component.html'
})
export class AppTopbarComponent {
  currentTheme: string = '';

  constructor(private themeService: ThemeService) {
    // Subscribe to theme changes
    this.themeService.getThemeSettings().subscribe(settings => {
      if (settings && settings.accentColors) {
        this.currentTheme = this.detectThemeFromColor(settings.accentColors['--primary-color']);
      }
    });
  }

  private detectThemeFromColor(color: string): string {
    if (color === '#092cab') return 'cuneiform';
    if (color === '#F80421') return 'bitaxe';
    return 'default';
  }
}
```

### Style the Logo

Edit `src/app/layout/styles/layout/_topbar.scss`:

```scss
.layout-topbar {
    // Existing styles...
    
    &-logo-container {
        display: flex;
        align-items: center;
        padding: 0 1rem;
    }
    
    &-logo {
        max-height: 40px;
        width: auto;
        transition: opacity 0.3s ease;
        
        &:hover {
            opacity: 0.8;
        }
    }
}
```

---

## Part 3: Customising Text/Branding

### Change "AxeOS" to "Cuneiform Miner"

#### Option 1: Page Title (Browser Tab)

```bash
nano src/index.html
```

Change:
```html
<title>AxeOS</title>
```

To:
```html
<title>Cuneiform Miner</title>
```

#### Option 2: Application Name in UI

Search for "AxeOS" references:

```bash
cd ~/Work/ESP-Miner/main/http_server/axe-os
grep -r "AxeOS\|axeos" src --include="*.html" --include="*.ts"
```

Replace occurrences as needed.

---

## Part 4: Customising the Footer

### Find Footer Component

```bash
ls -la src/app/layout/app.footer.component.*
```

### Edit Footer HTML

```bash
nano src/app/layout/app.footer.component.html
```

Example footer with Cuneiform branding:

```html
<div class="layout-footer">
    <div class="footer-content">
        <!-- Logo -->
        <div class="footer-logo">
            <img src="assets/logo-cuneiform.svg" alt="Cuneiform" height="30">
        </div>
        
        <!-- Branding Text -->
        <div class="footer-text">
            <span>Powered by <strong>Cuneiform</strong></span>
            <span class="footer-separator">|</span>
            <span>Version {{version}}</span>
        </div>
        
        <!-- Links -->
        <div class="footer-links">
            <a href="https://cuneiform.eu" target="_blank">Website</a>
            <a href="https://github.com/cuneiform" target="_blank">GitHub</a>
        </div>
    </div>
</div>
```

### Style the Footer

```bash
nano src/app/layout/styles/layout/_footer.scss
```

```scss
.layout-footer {
    background-color: var(--surface-card);
    border-top: 1px solid var(--surface-border);
    padding: 1rem 2rem;
    
    .footer-content {
        display: flex;
        align-items: center;
        justify-content: space-between;
        flex-wrap: wrap;
        gap: 1rem;
    }
    
    .footer-logo {
        img {
            max-height: 30px;
            width: auto;
        }
    }
    
    .footer-text {
        display: flex;
        align-items: center;
        gap: 0.5rem;
        font-size: 0.875rem;
        color: var(--text-color-secondary);
        
        .footer-separator {
            opacity: 0.5;
        }
    }
    
    .footer-links {
        display: flex;
        gap: 1rem;
        
        a {
            color: var(--primary-color);
            text-decoration: none;
            font-size: 0.875rem;
            transition: opacity 0.2s;
            
            &:hover {
                opacity: 0.8;
                text-decoration: underline;
            }
        }
    }
}
```

---

## Part 5: Custom Favicon

### Create Favicon

From your Cuneiform logo, create a favicon:

**Online Tool**: https://favicon.io/

Or using ImageMagick:
```bash
convert cuneiform-logo.png -resize 32x32 favicon.ico
```

### Replace Favicon

```bash
cd ~/Work/ESP-Miner/main/http_server/axe-os
cp /path/to/cuneiform-favicon.ico src/favicon.ico
```

---

## Part 6: Conditional Branding (Theme-Based)

If you want different logos for different themes:

### In TypeScript Component

```typescript
export class AppTopbarComponent {
  currentTheme: string = 'cuneiform';
  
  get logoPath(): string {
    switch(this.currentTheme) {
      case 'cuneiform':
        return 'assets/logo-cuneiform.svg';
      case 'bitaxe':
        return 'assets/logo-bitaxe.svg';
      default:
        return 'assets/logo-default.svg';
    }
  }
}
```

### In HTML Template

```html
<img [src]="logoPath" alt="Logo" height="40">
```

---

## Part 7: Build and Deploy

### Build Web UI

```bash
cd ~/Work/ESP-Miner/main/http_server/axe-os

# Build
npm run build
```

### Build and Flash Firmware

```bash
cd ~/Work/ESP-Miner

# Set up environment
cd ~/esp/esp-idf && . ./export.sh && cd ~/Work/ESP-Miner

# Build and flash
./build.sh -f -p /dev/cu.usbmodem141201
```

---

## Part 8: Testing

1. **Access UI**: `http://[DEVICE_IP]`
2. **Check topbar**: Logo should appear
3. **Check footer**: Branding should show
4. **Check browser tab**: Icon and title updated
5. **Test theme switching**: If conditional, verify logo changes

---

## File Locations Reference

```
main/http_server/axe-os/
├── src/
│   ├── index.html                              # Page title
│   ├── favicon.ico                             # Browser icon
│   ├── assets/
│   │   ├── logo-cuneiform.svg                 # Your logo
│   │   └── logo-cuneiform.png                 # PNG alternative
│   └── app/
│       ├── layout/
│       │   ├── app.topbar.component.html      # Header HTML
│       │   ├── app.topbar.component.ts        # Header logic
│       │   ├── app.footer.component.html      # Footer HTML
│       │   ├── app.footer.component.ts        # Footer logic
│       │   └── styles/
│       │       └── layout/
│       │           ├── _topbar.scss           # Header styles
│       │           └── _footer.scss           # Footer styles
```

---

## Quick Commands Reference

```bash
# Add logo
cp /path/to/logo.svg main/http_server/axe-os/src/assets/logo-cuneiform.svg

# Edit topbar
nano main/http_server/axe-os/src/app/layout/app.topbar.component.html

# Edit footer
nano main/http_server/axe-os/src/app/layout/app.footer.component.html

# Build web UI
cd main/http_server/axe-os && npm run build

# Build and flash
cd ~/Work/ESP-Miner && ./build.sh -f
```

---

## Troubleshooting

### Logo Not Appearing

1. **Check file path**: Ensure logo is in `src/assets/`
2. **Check build output**: `ls dist/axe-os/assets/`
3. **Clear browser cache**: Cmd+Shift+R
4. **Check console**: F12 → Console for 404 errors

### Logo Too Large/Small

Adjust in HTML:
```html
<img src="..." height="40">  <!-- Change height -->
```

Or in CSS:
```scss
.layout-topbar-logo {
    max-height: 40px;  // Adjust as needed
    width: auto;
}
```

### Logo Not Showing on Dark Theme

Ensure logo has:
- Transparent background
- Light colours that work on dark backgrounds
- Or use different logos per theme

---

## Advanced: SVG Logo Inline

For better performance, you can inline small SVG logos:

```html
<svg class="layout-topbar-logo" viewBox="0 0 100 50" height="40">
    <!-- Your SVG path data here -->
    <path d="M10,10 L90,10..." fill="currentColor"/>
</svg>
```

Benefits:
- No separate HTTP request
- Can be styled with CSS
- Scales perfectly

---

## Commit Your Changes

```bash
cd ~/Work/ESP-Miner

git add main/http_server/axe-os/src/assets/logo-cuneiform.svg
git add main/http_server/axe-os/src/app/layout/app.topbar.component.html
git add main/http_server/axe-os/src/app/layout/app.footer.component.html
git add main/http_server/axe-os/src/favicon.ico
git add main/http_server/axe-os/src/index.html

git commit -m "Add Cuneiform branding: logo, favicon, and updated headers"
git push origin feature/custom-branding
```

---

**Next**: Review the changes in your web UI and verify branding appears correctly!
