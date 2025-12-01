# MCP9808 Temperature Sensor Tutorial - Part 4: Angular Web Application

**Learning Goals**: Understanding Angular architecture, TypeScript patterns, RxJS reactive programming, real-time data visualization with Chart.js, and building production-ready web interfaces.

---

## Table of Contents

**Section A**: Angular Fundamentals and Project Setup
**Section B**: TypeScript and Component Architecture
**Section C**: Services and HTTP Communication
**Section D**: RxJS and Reactive Programming
**Section E**: Building the Temperature Gauge
**Section F**: Real-time Chart with Chart.js
**Section G**: Material Design and UX
**Section H**: Build and Deployment

---

# Section A: Angular Fundamentals

## What is Angular?

Angular is a **component-based framework** for building web applications.

### Key Concepts

**1. Components**: Building blocks of UI
```
Component = Template (HTML) + Class (TypeScript) + Styles (CSS/SCSS)
```

**2. Services**: Shared logic and data
```
Service = Reusable business logic (API calls, state management)
```

**3. Dependency Injection**: Automatic service provision
```
Angular creates and provides services when needed
```

**4. Reactive Programming**: Data streams with RxJS
```
Data flows through Observables (like event emitters)
```

### Architecture Overview

```
┌─────────────────────────────────────────────────┐
│              Angular Application                 │
├─────────────────────────────────────────────────┤
│                                                  │
│  ┌──────────────┐       ┌──────────────┐       │
│  │  Component   │       │   Service    │       │
│  │  (View)      │◄──────┤  (Logic)     │       │
│  └──────────────┘       └──────────────┘       │
│         │                       │               │
│         │ Uses                  │ HTTP          │
│         │                       │               │
│  ┌──────▼──────┐       ┌───────▼──────┐       │
│  │  Template   │       │  ESP32 API   │       │
│  │  (HTML)     │       │ /api/temp    │       │
│  └─────────────┘       └──────────────┘       │
│                                                  │
└─────────────────────────────────────────────────┘
```

**Data flow**:
1. User sees Component (HTML template)
2. Component uses Service for data
3. Service calls ESP32 HTTP API
4. Data flows back through Observable
5. Component updates view

---

## Project Setup

### Prerequisites Check

```bash
# Check Node.js (need 18.x or later)
node --version
# Should show: v18.x.x or v20.x.x

# Check npm
npm --version
# Should show: 9.x.x or 10.x.x

# If missing, install from: https://nodejs.org/
```

### Create Angular Project

```bash
cd ~/Work/temperature-monitor

# Create webapp directory
mkdir webapp
cd webapp

# Install Angular CLI globally
npm install -g @angular/cli

# Create new Angular project
ng new temp-monitor

# Answer prompts:
# ? Would you like to add Angular routing? → Yes
# ? Which stylesheet format would you like to use? → SCSS
```

**What just happened?**

```
ng new creates:
├── node_modules/        # Dependencies (don't commit)
├── src/                 # Source code (our work goes here)
│   ├── app/            # Application code
│   ├── assets/         # Images, fonts
│   ├── index.html      # Main HTML
│   └── main.ts         # Application entry point
├── angular.json        # Angular configuration
├── package.json        # Dependencies list
└── tsconfig.json       # TypeScript configuration
```

### Understanding package.json

```json
{
  "name": "temp-monitor",
  "version": "0.0.0",
  "scripts": {
    "ng": "ng",
    "start": "ng serve",      // Development server
    "build": "ng build",      // Production build
    "test": "ng test"         // Unit tests
  },
  "dependencies": {
    "@angular/core": "^17.0.0",     // Core Angular
    "@angular/common": "^17.0.0",   // Common utilities
    "rxjs": "~7.8.0",               // Reactive programming
    "zone.js": "~0.14.2"            // Change detection
  }
}
```

**Key scripts**:
- `npm start`: Runs development server (http://localhost:4200)
- `npm run build`: Creates production files in `dist/`
- `npm test`: Runs unit tests

### Test the Setup

```bash
cd temp-monitor

# Start development server
npm start

# Open browser to: http://localhost:4200
# You should see default Angular page
```

**What's happening?**
1. Angular CLI compiles TypeScript → JavaScript
2. Bundles all files together
3. Starts web server on port 4200
4. Watches for file changes (auto-reload)

---

# Section B: TypeScript and Component Architecture

## Understanding TypeScript

TypeScript = JavaScript + Types

### Why TypeScript?

```typescript
// JavaScript (runtime errors)
let temp = "25.5";
let doubled = temp * 2;  // NaN - bug!

// TypeScript (compile-time errors)
let temp: number = "25.5";  // ❌ Error: Type 'string' not assignable to 'number'
let doubled = temp * 2;     // ✓ OK - temp is number
```

**Benefits**:
- Catch errors before runtime
- Better IDE support (autocomplete)
- Self-documenting code
- Refactoring safety

### TypeScript Basics

**1. Basic Types**:

```typescript
// Primitives
let temperature: number = 25.5;
let sensorName: string = "MCP9808";
let isAvailable: boolean = true;
let timestamp: number = Date.now();

// Arrays
let readings: number[] = [24.5, 25.0, 25.5];
let sensors: string[] = ["sensor1", "sensor2"];

// Any (avoid when possible)
let unknownValue: any = "could be anything";
```

**2. Interfaces** (shape of objects):

```typescript
// Define structure
interface TemperatureData {
    temperature: number;
    available: boolean;
    lastUpdate: number;
    readCount: number;
    errorCount: number;
}

// Use it
function displayTemp(data: TemperatureData) {
    console.log(`Temp: ${data.temperature}°C`);
    // IDE knows data has these properties
}

// Create object matching interface
const reading: TemperatureData = {
    temperature: 25.5,
    available: true,
    lastUpdate: Date.now(),
    readCount: 42,
    errorCount: 0
};
```

**3. Optional Properties**:

```typescript
interface SensorConfig {
    address: number;
    name?: string;        // Optional (can be undefined)
    pollInterval?: number;
}

// Valid configurations
const config1: SensorConfig = { address: 0x18 };
const config2: SensorConfig = { 
    address: 0x19, 
    name: "Room Sensor" 
};
```

**4. Functions**:

```typescript
// Function with types
function convertToFahrenheit(celsius: number): number {
    return (celsius * 9/5) + 32;
}

// Arrow function
const toFahrenheit = (celsius: number): number => (celsius * 9/5) + 32;

// Optional parameters
function logTemp(temp: number, unit?: string): void {
    const u = unit || 'C';  // Default to 'C'
    console.log(`${temp}°${u}`);
}

logTemp(25);        // "25°C"
logTemp(77, 'F');   // "77°F"
```

**5. Classes**:

```typescript
class TemperatureSensor {
    // Properties
    private address: number;
    public name: string;
    
    // Constructor
    constructor(address: number, name: string) {
        this.address = address;
        this.name = name;
    }
    
    // Method
    public getAddress(): number {
        return this.address;
    }
    
    // Static method
    static fromConfig(config: SensorConfig): TemperatureSensor {
        return new TemperatureSensor(
            config.address, 
            config.name || 'Unknown'
        );
    }
}

// Usage
const sensor = new TemperatureSensor(0x18, "MCP9808");
console.log(sensor.name);           // ✓ OK - public
console.log(sensor.address);        // ❌ Error - private
console.log(sensor.getAddress());   // ✓ OK - public method
```

---

## Angular Components Explained

### Component Anatomy

A component has three parts:

```typescript
// 1. TypeScript Class (Logic)
@Component({
    selector: 'app-temperature-gauge',    // HTML tag name
    templateUrl: './gauge.component.html', // Template file
    styleUrls: ['./gauge.component.scss']  // Styles file
})
export class TemperatureGaugeComponent {
    // Properties (data)
    temperature: number = 0;
    
    // Methods (logic)
    getColor(): string {
        return this.temperature > 30 ? 'red' : 'blue';
    }
}
```

```html
<!-- 2. HTML Template (View) -->
<div class="gauge">
    <div class="value" [style.color]="getColor()">
        {{ temperature }}°C
    </div>
</div>
```

```scss
/* 3. SCSS Styles (Appearance) */
.gauge {
    width: 200px;
    height: 200px;
    
    .value {
        font-size: 48px;
        font-weight: bold;
    }
}
```

**How they connect**:
- Class defines `temperature` property
- Template displays `{{ temperature }}`
- Styles control appearance
- Changes in class automatically update view

### Component Lifecycle

Angular components have lifecycle hooks:

```typescript
export class MyComponent implements OnInit, OnDestroy {
    
    // 1. Constructor (component created)
    constructor() {
        console.log('Constructor called');
    }
    
    // 2. ngOnInit (component initialized)
    ngOnInit(): void {
        console.log('Component initialized');
        // Start timers, load data, etc.
    }
    
    // 3. ngOnDestroy (component destroyed)
    ngOnDestroy(): void {
        console.log('Component destroyed');
        // Clean up timers, unsubscribe, etc.
    }
}
```

**Lifecycle order**:
```
Constructor → ngOnInit → (component active) → ngOnDestroy
```

**When to use each**:
- **Constructor**: Dependency injection only
- **ngOnInit**: Load data, start timers
- **ngOnDestroy**: Clean up (prevent memory leaks!)

### Data Binding

**1. Interpolation** (component → template):

```typescript
// Component
export class MyComponent {
    temperature: number = 25.5;
    sensorName: string = "MCP9808";
}
```

```html
<!-- Template -->
<p>{{ temperature }}°C</p>
<p>Sensor: {{ sensorName }}</p>
```

**2. Property Binding** (component → template):

```typescript
// Component
export class MyComponent {
    imageUrl: string = '/assets/sensor.png';
    isDisabled: boolean = false;
}
```

```html
<!-- Template -->
<img [src]="imageUrl">
<button [disabled]="isDisabled">Click Me</button>
```

**Notice**: Square brackets `[]` mean "bind to property"

**3. Event Binding** (template → component):

```typescript
// Component
export class MyComponent {
    onButtonClick(): void {
        console.log('Button clicked!');
    }
    
    onInputChange(event: Event): void {
        const input = event.target as HTMLInputElement;
        console.log('Value:', input.value);
    }
}
```

```html
<!-- Template -->
<button (click)="onButtonClick()">Click Me</button>
<input (input)="onInputChange($event)">
```

**Notice**: Parentheses `()` mean "listen to event"

**4. Two-way Binding** (component ↔ template):

```typescript
// Component
export class MyComponent {
    userName: string = '';
}
```

```html
<!-- Template -->
<input [(ngModel)]="userName">
<p>Hello, {{ userName }}!</p>

<!-- As you type in input, userName updates -->
<!-- As userName changes, input displays new value -->
```

**Notice**: Banana-in-a-box `[()]` = two-way binding

### Component Communication

**Parent → Child** (Input):

```typescript
// Child component
@Component({
    selector: 'app-temperature-display'
})
export class TemperatureDisplayComponent {
    @Input() temperature: number = 0;  // Receives data from parent
    @Input() unit: string = 'C';
}
```

```typescript
// Parent component
@Component({
    template: `
        <app-temperature-display 
            [temperature]="currentTemp"
            [unit]="'C'">
        </app-temperature-display>
    `
})
export class ParentComponent {
    currentTemp: number = 25.5;
}
```

**Child → Parent** (Output):

```typescript
// Child component
@Component({
    selector: 'app-control-panel'
})
export class ControlPanelComponent {
    @Output() resetClicked = new EventEmitter<void>();
    
    onResetClick(): void {
        this.resetClicked.emit();  // Send event to parent
    }
}
```

```typescript
// Parent component
@Component({
    template: `
        <app-control-panel 
            (resetClicked)="handleReset()">
        </app-control-panel>
    `
})
export class ParentComponent {
    handleReset(): void {
        console.log('Reset requested by child');
    }
}
```

---

## Creating Our First Component

### Generate Component

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Generate temperature-gauge component
ng generate component components/temperature-gauge

# Short form:
ng g c components/temperature-gauge
```

**What this creates**:

```
src/app/components/temperature-gauge/
├── temperature-gauge.component.ts       # TypeScript class
├── temperature-gauge.component.html     # HTML template
├── temperature-gauge.component.scss     # Styles
└── temperature-gauge.component.spec.ts  # Tests (ignore for now)
```

**Also updates** `app.module.ts` to register component!

### Understanding the Generated Code

**`temperature-gauge.component.ts`**:

```typescript
import { Component } from '@angular/core';

@Component({
    selector: 'app-temperature-gauge',
    templateUrl: './temperature-gauge.component.html',
    styleUrls: ['./temperature-gauge.component.scss']
})
export class TemperatureGaugeComponent {
    // Component logic goes here
}
```

**Line-by-line**:

1. **Import**: Get Component decorator from Angular core
2. **@Component**: Decorator that marks class as component
3. **selector**: HTML tag name (`<app-temperature-gauge>`)
4. **templateUrl**: Path to HTML template
5. **styleUrls**: Array of style files (can have multiple)
6. **export class**: The component class (our code goes here)

### Add Component Logic

Edit `temperature-gauge.component.ts`:

```typescript
import { Component, Input } from '@angular/core';

@Component({
    selector: 'app-temperature-gauge',
    templateUrl: './temperature-gauge.component.html',
    styleUrls: ['./temperature-gauge.component.scss']
})
export class TemperatureGaugeComponent {
    // Input properties (received from parent)
    @Input() temperature: number = 0;
    @Input() isAvailable: boolean = false;
    
    /**
     * Calculate gauge needle rotation
     * Maps temperature (-40 to 125°C) to rotation (-135° to 135°)
     */
    getGaugeRotation(): string {
        const minTemp = -40;
        const maxTemp = 125;
        const minDeg = -135;
        const maxDeg = 135;
        
        // Clamp temperature to valid range
        const clampedTemp = Math.max(minTemp, Math.min(maxTemp, this.temperature));
        
        // Linear interpolation
        const rotation = ((clampedTemp - minTemp) / (maxTemp - minTemp)) 
                        * (maxDeg - minDeg) + minDeg;
        
        return `rotate(${rotation}deg)`;
    }
    
    /**
     * Get color based on temperature
     */
    getTemperatureColor(): string {
        if (!this.isAvailable) return '#9e9e9e';  // Grey when offline
        if (this.temperature < 0) return '#2196f3';   // Blue (cold)
        if (this.temperature < 20) return '#03a9f4';  // Light blue
        if (this.temperature < 30) return '#4caf50';  // Green (normal)
        if (this.temperature < 40) return '#ff9800';  // Orange (warm)
        return '#f44336';  // Red (hot)
    }
}
```

**Key concepts**:

1. **@Input()**: Receives data from parent component
   ```typescript
   @Input() temperature: number = 0;  // Default value
   ```

2. **Methods return values**: Used in template
   ```typescript
   getGaugeRotation(): string {  // Returns CSS transform
       return `rotate(${angle}deg)`;
   }
   ```

3. **Conditional logic**: Based on component state
   ```typescript
   if (!this.isAvailable) return '#9e9e9e';
   ```

4. **Math operations**: Calculate visual properties
   ```typescript
   const rotation = ((temp - min) / (max - min)) * (degMax - degMin) + degMin;
   ```

---

# Section C: Services and HTTP Communication

## What is a Service?

**Service** = Reusable business logic shared across components.

**Why services?**
- **Separation of concerns**: Components handle UI, services handle data
- **Reusability**: Multiple components use same service
- **Testability**: Easy to mock services in tests
- **Singleton**: One instance shared across app

### Service Example

```typescript
// Without service (BAD)
export class Component1 {
    temperature: number;
    
    loadData() {
        fetch('http://192.168.4.1/api/temperature')
            .then(res => res.json())
            .then(data => this.temperature = data.temperature);
    }
}

export class Component2 {
    // Duplicate the same code! 😞
    loadData() {
        fetch('http://192.168.4.1/api/temperature')
            .then(res => res.json())
            .then(data => { /* ... */ });
    }
}
```

```typescript
// With service (GOOD)
@Injectable()
export class TemperatureService {
    getTemperature() {
        return fetch('http://192.168.4.1/api/temperature')
            .then(res => res.json());
    }
}

export class Component1 {
    constructor(private tempService: TemperatureService) {}
    
    loadData() {
        this.tempService.getTemperature()
            .then(data => this.temperature = data.temperature);
    }
}

export class Component2 {
    constructor(private tempService: TemperatureService) {}
    
    loadData() {
        // Same service, no duplication! 😊
        this.tempService.getTemperature()
            .then(data => { /* ... */ });
    }
}
```

---

## Creating Temperature Service

### Generate Service

```bash
ng generate service services/temperature

# Short form:
ng g s services/temperature
```

**Creates**:
```
src/app/services/
├── temperature.service.ts
└── temperature.service.spec.ts
```

### Define Data Interfaces

First, define TypeScript interfaces for type safety.

Create `src/app/models/temperature-data.ts`:

```typescript
/**
 * Temperature data from ESP32 API
 */
export interface TemperatureData {
    temperature: number;     // Current temperature in °C
    available: boolean;      // Is sensor responding?
    lastUpdate: number;      // Timestamp (milliseconds)
    dataAge: number;        // Age of data (milliseconds)
    readCount: number;      // Total successful reads
    errorCount: number;     // Total failed reads
}

/**
 * Temperature history from ESP32 API
 */
export interface HistoryData {
    history: number[];      // Array of temperature readings
}
```

**Why interfaces?**
```typescript
// TypeScript catches errors:
const data: TemperatureData = {
    temperature: 25.5,
    available: true,
    lastUpdate: Date.now(),
    // ❌ Error: Missing 'dataAge', 'readCount', 'errorCount'
};

// IDE autocomplete knows properties:
console.log(data.temperature);  // ✓ Autocomplete suggests properties
console.log(data.temp);         // ❌ Error: Property 'temp' does not exist
```

### Implement Service

Edit `services/temperature.service.ts`:

```typescript
import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, interval } from 'rxjs';
import { switchMap, startWith, catchError, retry } from 'rxjs/operators';
import { of } from 'rxjs';
import { TemperatureData, HistoryData } from '../models/temperature-data';

/**
 * Service for communicating with ESP32 temperature API
 */
@Injectable({
    providedIn: 'root'  // Singleton service available everywhere
})
export class TemperatureService {
    // ESP32 IP address (change if different)
    private baseUrl = 'http://192.168.4.1';
    
    /**
     * Constructor - Angular automatically injects HttpClient
     * 
     * @param http HTTP client for making requests
     */
    constructor(private http: HttpClient) {
        console.log('TemperatureService created');
    }
    
    /**
     * Get current temperature (single request)
     * 
     * @returns Observable of TemperatureData
     * 
     * Usage:
     *   this.temperatureService.getCurrentTemperature()
     *       .subscribe(data => console.log(data.temperature));
     */
    getCurrentTemperature(): Observable<TemperatureData> {
        const url = `${this.baseUrl}/api/temperature`;
        
        return this.http.get<TemperatureData>(url).pipe(
            retry(2),  // Retry up to 2 times on failure
            catchError(error => {
                console.error('Failed to fetch temperature:', error);
                // Return default data on error
                return of({
                    temperature: 0,
                    available: false,
                    lastUpdate: 0,
                    dataAge: 0,
                    readCount: 0,
                    errorCount: 0
                });
            })
        );
    }
    
    /**
     * Get temperature history
     * 
     * @returns Observable of HistoryData
     */
    getHistory(): Observable<HistoryData> {
        const url = `${this.baseUrl}/api/history`;
        
        return this.http.get<HistoryData>(url).pipe(
            catchError(error => {
                console.error('Failed to fetch history:', error);
                return of({ history: [] });
            })
        );
    }
    
    /**
     * Get temperature stream that auto-updates every 2 seconds
     * 
     * @returns Observable that emits TemperatureData every 2s
     * 
     * Usage:
     *   this.temperatureService.getTemperatureStream()
     *       .subscribe(data => this.temperature = data.temperature);
     * 
     * Note: Remember to unsubscribe in ngOnDestroy!
     */
    getTemperatureStream(): Observable<TemperatureData> {
        return interval(2000).pipe(  // Emit every 2000ms
            startWith(0),            // Emit immediately (don't wait 2s)
            switchMap(() => this.getCurrentTemperature())  // Get temp on each emit
        );
    }
}
```

**Line-by-line explanation**:

1. **@Injectable()**: Marks class as injectable service
   ```typescript
   @Injectable({
       providedIn: 'root'  // Singleton - one instance for whole app
   })
   ```

2. **Constructor injection**: Angular provides HttpClient
   ```typescript
   constructor(private http: HttpClient) {}
   // Angular sees HttpClient needed and provides it
   ```

3. **Observable**: Async data stream
   ```typescript
   getCurrentTemperature(): Observable<TemperatureData> {
       // Returns Observable (not the data itself)
   }
   ```

4. **http.get()**: Make HTTP GET request
   ```typescript
   this.http.get<TemperatureData>(url)
   // Generic type <TemperatureData> ensures type safety
   ```

5. **pipe()**: Chain operators
   ```typescript
   .pipe(
       retry(2),        // Operator 1: Retry on failure
       catchError(...)  // Operator 2: Handle errors
   )
   ```

6. **catchError()**: Error handling
   ```typescript
   catchError(error => {
       console.error('Error:', error);
       return of(defaultData);  // Return default data
   })
   ```

### Register HttpClient

Angular needs to know we're using HTTP.

Edit `app.module.ts`:

```typescript
import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { HttpClientModule } from '@angular/common/http';  // ← Add this

import { AppComponent } from './app.component';

@NgModule({
    declarations: [
        AppComponent,
        // ... components
    ],
    imports: [
        BrowserModule,
        HttpClientModule,  // ← Add this
    ],
    providers: [],
    bootstrap: [AppComponent]
})
export class AppModule { }
```

**Why import HttpClientModule?**
- Provides HttpClient for dependency injection
- Registers HTTP interceptors
- Configures HTTP behavior

---

# Section D: RxJS and Reactive Programming

## What is RxJS?

**RxJS** = Reactive Extensions for JavaScript

**Concept**: Everything is a stream of data over time.

```
Temperature readings over time:
───25.5°C───25.3°C───25.7°C───25.4°C───>

User clicks over time:
───click───────click───click────────────>

HTTP responses over time:
───{data}────────{data}──────{data}────>
```

### Observable vs Promise

**Promise** (single value):
```typescript
// Returns ONE value, then done
fetch('/api/temperature')
    .then(response => response.json())
    .then(data => console.log(data));
```

**Observable** (stream of values):
```typescript
// Can emit MULTIPLE values over time
interval(2000)
    .subscribe(value => console.log(value));
// Outputs: 0, 1, 2, 3, 4, ... (every 2 seconds)
```

**Key differences**:

| Feature | Promise | Observable |
|---------|---------|------------|
| Values | One | Zero to infinite |
| Lazy | No (executes immediately) | Yes (only when subscribed) |
| Cancellable | No | Yes (unsubscribe) |
| Operators | then, catch | map, filter, switchMap, etc. |

### Creating Observables

**1. From value**:
```typescript
import { of } from 'rxjs';

const data$ = of(25.5);  // $ suffix = Observable convention
data$.subscribe(value => console.log(value));  // Output: 25.5
```

**2. From array**:
```typescript
import { of } from 'rxjs';

const temps$ = of(24.5, 25.0, 25.5);
temps$.subscribe(value => console.log(value));
// Output: 24.5, 25.0, 25.5 (emitted separately)
```

**3. From interval**:
```typescript
import { interval } from 'rxjs';

const timer$ = interval(1000);  // Emit every 1000ms
timer$.subscribe(value => console.log(value));
// Output: 0, 1, 2, 3, ... (every second)
```

**4. From HTTP request**:
```typescript
this.http.get('/api/temperature')  // Returns Observable
    .subscribe(data => console.log(data));
```

### Subscribing to Observables

```typescript
const subscription = observable$.subscribe({
    next: (value) => {
        console.log('Received:', value);
    },
    error: (error) => {
        console.error('Error:', error);
    },
    complete: () => {
        console.log('Complete!');
    }
});

// Later: Unsubscribe to prevent memory leaks
subscription.unsubscribe();
```

**Shorthand** (if only need next):
```typescript
observable$.subscribe(value => console.log(value));
```

### RxJS Operators

Operators transform Observables.

**1. map** (transform values):
```typescript
import { map } from 'rxjs/operators';

// Convert Celsius to Fahrenheit
temperature$.pipe(
    map(celsius => (celsius * 9/5) + 32)
).subscribe(fahrenheit => console.log(fahrenheit));

// Input:  25°C
// Output: 77°F
```

**2. filter** (select values):
```typescript
import { filter } from 'rxjs/operators';

// Only values > 30
temperature$.pipe(
    filter(temp => temp > 30)
).subscribe(temp => console.log('Hot!', temp));

// Input:  25, 28, 32, 35, 29
// Output: 32, 35
```

**3. switchMap** (switch to new Observable):
```typescript
import { switchMap } from 'rxjs/operators';

// Every 2s, get current temperature
interval(2000).pipe(
    switchMap(() => this.http.get('/api/temperature'))
).subscribe(data => console.log(data));

// Explanation:
// 1. interval emits 0, 1, 2, ...
// 2. For each emit, switchMap makes HTTP request
// 3. Subscribe gets HTTP response data
```

**Why "switch"?**
- Cancels previous request if new one starts
- Prevents overlapping requests
- Example: User types fast, only search for latest input

**4. startWith** (emit value immediately):
```typescript
import { startWith } from 'rxjs/operators';

interval(2000).pipe(
    startWith(0)  // Emit 0 immediately
).subscribe(value => console.log(value));

// Without startWith: wait 2s → 0 → 1 → 2 → ...
// With startWith:    0 → wait 2s → 1 → 2 → ...
```

**5. catchError** (error handling):
```typescript
import { catchError } from 'rxjs/operators';
import { of } from 'rxjs';

this.http.get('/api/temperature').pipe(
    catchError(error => {
        console.error('Error:', error);
        return of({ temperature: 0 });  // Return default value
    })
).subscribe(data => console.log(data));
```

**6. retry** (retry on error):
```typescript
import { retry } from 'rxjs/operators';

this.http.get('/api/temperature').pipe(
    retry(3)  // Retry up to 3 times
).subscribe(data => console.log(data));
```

### Combining Operators

```typescript
// Real-world example: Auto-refreshing temperature
interval(2000).pipe(
    startWith(0),                    // Start immediately
    switchMap(() => this.http.get('/api/temperature')),  // Get data
    map(data => data.temperature),   // Extract temperature
    filter(temp => temp > 0),        // Ignore invalid readings
    catchError(error => of(0))       // Return 0 on error
).subscribe(temp => {
    console.log('Temperature:', temp);
});
```

**Flow**:
```
interval(2000)
    ↓ every 2s emits: 0, 1, 2, ...
startWith(0)
    ↓ adds initial: 0, 0, 1, 2, ...
switchMap(() => http.get(...))
    ↓ each emit triggers: {temperature: 25.5, ...}
map(data => data.temperature)
    ↓ extract value: 25.5
filter(temp => temp > 0)
    ↓ only positive: 25.5 (if > 0)
subscribe(temp => ...)
    ↓ final value: 25.5
```

---

## Using Service in Component

### Inject Service

Edit `app.component.ts`:

```typescript
import { Component, OnInit, OnDestroy } from '@angular/core';
import { Subscription } from 'rxjs';
import { TemperatureService } from './services/temperature.service';
import { TemperatureData } from './models/temperature-data';

@Component({
    selector: 'app-root',
    templateUrl: './app.component.html',
    styleUrls: ['./app.component.scss']
})
export class AppComponent implements OnInit, OnDestroy {
    title = 'ESP32 Temperature Monitor';
    temperatureData?: TemperatureData;  // ? means optional
    private subscription?: Subscription;
    
    /**
     * Constructor - Angular injects TemperatureService
     */
    constructor(private temperatureService: TemperatureService) {
        console.log('AppComponent created');
    }
    
    /**
     * Lifecycle hook - called after component initialized
     */
    ngOnInit(): void {
        console.log('AppComponent initialized - starting data stream');
        
        // Subscribe to temperature stream
        this.subscription = this.temperatureService
            .getTemperatureStream()
            .subscribe({
                next: (data) => {
                    this.temperatureData = data;
                    console.log('Received temperature:', data.temperature);
                },
                error: (error) => {
                    console.error('Error in temperature stream:', error);
                }
            });
    }
    
    /**
     * Lifecycle hook - called before component destroyed
     * CRITICAL: Unsubscribe to prevent memory leaks!
     */
    ngOnDestroy(): void {
        console.log('AppComponent destroyed - cleaning up');
        
        if (this.subscription) {
            this.subscription.unsubscribe();
        }
    }
}
```

**Key concepts**:

1. **Dependency Injection**:
   ```typescript
   constructor(private temperatureService: TemperatureService) {}
   // Angular creates service and provides it
   ```

2. **Subscribe in ngOnInit**:
   ```typescript
   ngOnInit(): void {
       this.subscription = observable$.subscribe(...);
   }
   // Not in constructor - component not fully initialized yet
   ```

3. **Unsubscribe in ngOnDestroy**:
   ```typescript
   ngOnDestroy(): void {
       this.subscription?.unsubscribe();
   }
   // Prevent memory leaks - subscription keeps running otherwise!
   ```

4. **Optional chaining** (`?.`):
   ```typescript
   this.subscription?.unsubscribe();
   // Only calls unsubscribe if subscription exists
   // Same as: if (this.subscription) { this.subscription.unsubscribe(); }
   ```

---

## Questions to Reinforce Learning

1. **What's the difference between a Component and a Service?**
   <details>
   <summary>Answer</summary>
   - Component: UI logic, displays data, handles user interaction
   - Service: Business logic, data fetching, shared across components
   - Components are created/destroyed frequently
   - Services are typically singletons (one instance)
   </details>

2. **Why use TypeScript interfaces?**
   <details>
   <summary>Answer</summary>
   - Type safety (catch errors at compile time)
   - IDE autocomplete and IntelliSense
   - Self-documenting code (interface shows structure)
   - Refactoring safety (rename properties easily)
   - No runtime cost (interfaces are compile-time only)
   </details>

3. **What happens if you forget to unsubscribe?**
   <details>
   <summary>Answer</summary>
   - Memory leak (subscription keeps running)
   - Component reference not garbage collected
   - Callback continues executing (updates destroyed component)
   - Multiple subscriptions if component recreated
   - Eventually app slows down / crashes
   </details>

4. **When would you use switchMap vs mergeMap?**
   <details>
   <summary>Answer</summary>
   - **switchMap**: Cancel previous Observable when new one arrives
     - Use for: HTTP requests, search (only want latest)
   - **mergeMap**: Keep all Observables running concurrently
     - Use for: Independent operations, parallel requests
   </details>

5. **Why does `getTemperatureStream()` use `interval` + `switchMap`?**
   <details>
   <summary>Answer</summary>
   - `interval(2000)`: Emit value every 2 seconds (timer)
   - `switchMap`: Convert each timer emit into HTTP request
   - Result: HTTP request every 2 seconds
   - `switchMap` (not `mergeMap`): Cancel previous request if still running
   </details>

---

**Continue to Section E** where we'll build the visual gauge component with CSS animations and learn about Angular's change detection!

Would you like me to continue with Sections E-H, or would you like to practice what we've covered so far?
