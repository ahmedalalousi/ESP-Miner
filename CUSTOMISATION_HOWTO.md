# Cuneiform Theme Customisation Guide

Guide for adding Cuneiform branding as a selectable theme in the ESP-Miner AxeOS interface.

## Overview

This guide will help you:
1. Add a new "Cuneiform" theme alongside the existing Bitaxe theme
2. Make it selectable via the UI design page
3. Add your logo
4. Configure Cuneiform brand colours

## Directory Structure

```
main/http_server/axe-os/
├── src/
│   ├── index.html                           # Page title
│   ├── favicon.ico                          # Browser icon
│   ├── assets/
│   │   └── logo-cuneiform.png              # Your logo (add this)
│   └── app/
│       ├── layout/styles/theme/themes/vela/
│       │   ├── bitaxe/                     # Existing Bitaxe theme
│       │   │   ├── _variables.scss
│       │   │   ├── _fonts.scss
│       │   │   ├── _extensions.scss
│       │   │   └── theme.scss
│       │   └── cuneiform/                  # New Cuneiform theme (create this)
│       │       ├── _variables.scss
│       │       ├── _fonts.scss
│       │       ├── _extensions.scss
│       │       └── theme.scss
│       └── components/
│           └── design/                      # Theme selector UI
│               ├── design-component.ts
│               └── design-component.html
```

---

## Step 1: Create Cuneiform Theme Directory

```bash
cd main/http_server/axe-os

# Create the Cuneiform theme directory
mkdir -p src/app/layout/styles/theme/themes/vela/cuneiform

# Navigate to it
cd src/app/layout/styles/theme/themes/vela/cuneiform
```

---

## Step 2: Create Theme Files

### File 1: `_variables.scss`

Create `src/app/layout/styles/theme/themes/vela/cuneiform/_variables.scss`:

```scss
// Cuneiform Brand Colours
$primaryColor: #092cab !default;  // Cuneiform deep blue
$primaryLightColor: scale-color($primaryColor, $lightness: 30%) !default;
$primaryDarkColor: scale-color($primaryColor, $lightness: -10%) !default;
$primaryDarkerColor: scale-color($primaryColor, $lightness: -20%) !default;
$primaryTextColor: white !default;

// Secondary colours
$secondaryColor: #292929 !default;  // Cuneiform dark grey

// Highlight colours (for selections, focus states)
$highlightBg: rgba(9, 44, 171, .16) !default;  // Light blue tint
$highlightTextColor: rgba(255,255,255,.87) !default;
$highlightFocusBg: rgba($primaryColor, .24) !default;

// Input styling
$inputPadding: 0.5rem 0.75rem !default;

// Import parent variables
@import '../_variables';
```

### File 2: `_fonts.scss`

Create `src/app/layout/styles/theme/themes/vela/cuneiform/_fonts.scss`:

```scss
// Cuneiform custom fonts (if needed)
// Add any custom font imports here

// For now, inherit default fonts
```

### File 3: `_extensions.scss`

Create `src/app/layout/styles/theme/themes/vela/cuneiform/_extensions.scss`:

```scss
// Cuneiform theme extensions
// Additional custom styling beyond base theme

// Example: Custom button hover effects
.p-button:hover {
    box-shadow: 0 4px 12px rgba(9, 44, 171, 0.3);
}

// Example: Custom card styling
.p-card {
    border-left: 3px solid $primaryColor;
}
```

### File 4: `theme.scss`

Create `src/app/layout/styles/theme/themes/vela/cuneiform/theme.scss`:

```scss
@import './variables';
@import './_fonts';
@import '../../../theme-base/_components';
@import './_extensions';
```

---

## Step 3: Add Logo

```bash
# Copy your logo to assets
cd main/http_server/axe-os

# Add your logo (replace with your actual PNG file)
cp /path/to/your/cuneiform-logo.png src/assets/logo-cuneiform.png
```

**Recommended logo specifications**:
- Format: PNG with transparent background (SVG even better)
- Size: 200-400px wide
- Aspect ratio: Maintain brand proportions

---

## Step 4: Update Theme Selector UI

### Find the Design Component

```bash
cd main/http_server/axe-os

# Locate the design component files
ls -la src/app/components/design/
```

### Update `design-component.ts`

You need to add Cuneiform as a theme option. Look for the themes array/object and add:

```typescript
// Example structure (adapt to actual code):
themes = [
  {
    name: 'Bitaxe',
    value: 'bitaxe',
    primaryColor: '#f80421'
  },
  {
    name: 'Cuneiform',
    value: 'cuneiform',
    primaryColor: '#092cab'
  }
];
```

### Update `design-component.html`

Add Cuneiform to the theme selector dropdown/buttons. Look for theme selection UI and add:

```html
<!-- Example structure (adapt to actual code): -->
<button (click)="setTheme('cuneiform')">
  Cuneiform
</button>
```

---

## Step 5: Update Page Title (Optional)

If you want to show "Cuneiform Miner" when the Cuneiform theme is active:

Edit `src/index.html`:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>AxeOS - Miner Control</title>  <!-- Or make it dynamic -->
  <base href="/">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" type="image/x-icon" href="favicon.ico">
  <link rel="prefetch" href="/assets/fonts/Nippo-Regular.woff2" as="font" type="font/woff2">
  <link rel="prefetch" href="/assets/fonts/primeicons.woff2" as="font" type="font/woff2">
</head>
<body>
  <app-root></app-root>
</body>
</html>
```

---

## Step 6: Add Custom Favicon (Optional)

Replace the favicon with a Cuneiform-branded one:

```bash
# Replace favicon.ico with your Cuneiform icon
# Create a 32x32 or 16x16 ICO file
cp /path/to/cuneiform-favicon.ico src/favicon.ico
```

**Tools to create favicon**:
- Online: https://favicon.io/
- From PNG: Use ImageMagick or online converters

---

## Step 7: Register Theme in Angular

You'll need to ensure the theme is properly loaded. This typically involves:

1. **Check theme loading logic** in the app component or theme service
2. **Add Cuneiform theme import** to the build configuration

### Update `angular.json` (if needed)

Look for theme imports and add:

```json
"styles": [
  "src/styles.scss",
  "src/app/layout/styles/theme/themes/vela/bitaxe/theme.scss",
  "src/app/layout/styles/theme/themes/vela/cuneiform/theme.scss"
]
```

---

## Step 8: Build and Test

### Build the Web UI

```bash
cd main/http_server/axe-os

# Install dependencies (if needed)
npm install

# Build the UI
npm run build
```

### Build and Flash Firmware

```bash
# Go back to project root
cd ../../../..

# Build with your build script
./build.sh --clean --flash --monitor
```

### Test Theme Switching

1. Access the web UI: `http://192.168.1.65`
2. Navigate to: `http://192.168.1.65/#/design`
3. Select "Cuneiform" theme
4. Verify colours change correctly
5. Check that logo appears (if you added logo display)

---

## Step 9: Finding Where to Add Theme Logic

If you need to find where themes are implemented:

```bash
cd main/http_server/axe-os/src

# Search for theme-related code
grep -r "theme\|Theme" app/components/design/ --include="*.ts"

# Search for colour changes
grep -r "primaryColor\|primary-color" app/ --include="*.ts" --include="*.scss"

# Find where bitaxe theme is referenced
grep -r "bitaxe" app/ --include="*.ts" --include="*.html"
```

---

## Advanced Customisation

### Adding Logo to Header/Topbar

Find the topbar component:

```bash
find src -name "*topbar*" -type f
```

Edit the topbar HTML to include your logo:

```html
<img src="assets/logo-cuneiform.png" 
     alt="Cuneiform" 
     class="logo"
     *ngIf="currentTheme === 'cuneiform'">
```

### Dynamic Theme Loading

If themes should load dynamically based on user selection, you'll need:

1. **Theme service** to manage current theme
2. **Dynamic stylesheet loading** to switch between themes
3. **LocalStorage** to persist user's theme choice

Example theme service:

```typescript
import { Injectable } from '@angular/core';

@Injectable({
  providedIn: 'root'
})
export class ThemeService {
  currentTheme: string = 'bitaxe';

  setTheme(themeName: string) {
    this.currentTheme = themeName;
    localStorage.setItem('selectedTheme', themeName);
    // Load corresponding stylesheet
    this.loadThemeStylesheet(themeName);
  }

  loadThemeStylesheet(themeName: string) {
    const themeLink = document.getElementById('theme-css') as HTMLLinkElement;
    if (themeLink) {
      themeLink.href = `${themeName}-theme.css`;
    }
  }
}
```

---

## Troubleshooting

### Theme Not Appearing

1. **Check build output**: Ensure theme files were compiled
   ```bash
   ls -la dist/axe-os/  # After build
   ```

2. **Check browser console**: Look for CSS loading errors

3. **Clear browser cache**: Force refresh (Cmd+Shift+R on Mac)

### Colours Not Changing

1. **Verify SCSS variables** are correct
2. **Check CSS specificity** - ensure Cuneiform styles override defaults
3. **Inspect element** in browser DevTools to see which styles are applied

### Logo Not Displaying

1. **Check file path** in HTML/TypeScript
2. **Verify asset was included** in build output
3. **Check image file size** - optimize if too large

---

## File Checklist

Before building, ensure you've created/modified:

- [ ] `src/app/layout/styles/theme/themes/vela/cuneiform/_variables.scss`
- [ ] `src/app/layout/styles/theme/themes/vela/cuneiform/_fonts.scss`
- [ ] `src/app/layout/styles/theme/themes/vela/cuneiform/_extensions.scss`
- [ ] `src/app/layout/styles/theme/themes/vela/cuneiform/theme.scss`
- [ ] `src/assets/logo-cuneiform.png` (your logo)
- [ ] `src/app/components/design/design-component.ts` (add Cuneiform option)
- [ ] `src/app/components/design/design-component.html` (add UI for selection)
- [ ] `src/favicon.ico` (optional - Cuneiform favicon)
- [ ] `src/index.html` (optional - update title)

---

## Quick Commands Reference

```bash
# Navigate to AxeOS
cd main/http_server/axe-os

# Create theme directory
mkdir -p src/app/layout/styles/theme/themes/vela/cuneiform

# Create all theme files
touch src/app/layout/styles/theme/themes/vela/cuneiform/{_variables.scss,_fonts.scss,_extensions.scss,theme.scss}

# Add logo
cp /path/to/logo.png src/assets/logo-cuneiform.png

# Build UI
npm run build

# Build and flash firmware
cd ../../.. # Back to project root
./build.sh -c -f -m

# View design component
cat main/http_server/axe-os/src/app/components/design/design-component.ts
```

---

## Testing Your Theme

### Checklist

- [ ] Theme appears in design page selector
- [ ] Clicking theme changes primary colour to `#092cab`
- [ ] Buttons show Cuneiform blue on hover
- [ ] Highlighted items use Cuneiform blue
- [ ] Logo displays correctly (if implemented)
- [ ] Theme selection persists after page reload
- [ ] Theme selection persists after device reboot

---

## Next Steps

1. **Create the theme files** using the templates above
2. **Add your logo** to assets
3. **Find and update** the design component to add Cuneiform
4. **Build and test** locally
5. **Commit your changes**:
   ```bash
   git add .
   git commit -m "Add Cuneiform theme with custom branding"
   git push origin feature/custom-branding
   ```

---

## Need Help Finding Code?

If you need help locating where to add theme selection logic, run:

```bash
cd main/http_server/axe-os/src

# Show design component contents
cat app/components/design/design-component.ts

# Show design component HTML
cat app/components/design/design-component.html
```

Share the output and I'll tell you exactly where to add the Cuneiform theme option.

---

**Remember**: Always commit working code before making changes, so you can revert if needed!
