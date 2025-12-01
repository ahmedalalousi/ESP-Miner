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

# Section E: Building the Temperature Gauge

## CSS and SCSS Fundamentals

### What is SCSS?

**SCSS** (Sassy CSS) = CSS with superpowers

**CSS**:
```css
.gauge {
    width: 300px;
    height: 300px;
}

.gauge .needle {
    background: red;
}
```

**SCSS** (with nesting):
```scss
.gauge {
    width: 300px;
    height: 300px;
    
    .needle {  // Nested - compiles to .gauge .needle
        background: red;
    }
}
```

**SCSS features we'll use**:
1. **Nesting**: Organize styles hierarchically
2. **Variables**: Reuse values
3. **Calculations**: Math in CSS
4. **Parent selector** (`&`): Reference parent class

---

## Gauge Design Theory

### Visual Requirements

Our gauge needs:
1. **Circular body**: Container for gauge
2. **Temperature marks**: -40°, 0°, 25°, 50°, etc.
3. **Rotating needle**: Points to current temperature
4. **Center pivot**: Needle rotation point
5. **Temperature display**: Digital readout
6. **Status indicator**: Online/offline

### Mathematics: Temperature to Rotation

**Problem**: Map temperature (-40°C to 125°C) to rotation angle (-135° to 135°)

```
Temperature range: -40°C to 125°C (165° total)
Rotation range:   -135° to 135°  (270° total)

Formula:
rotation = ((temp - minTemp) / (maxTemp - minTemp)) × (maxDeg - minDeg) + minDeg

Example: 25°C
rotation = ((25 - (-40)) / (125 - (-40))) × (135 - (-135)) + (-135)
         = (65 / 165) × 270 - 135
         = 0.394 × 270 - 135
         = 106.36 - 135
         = -28.64°
```

**Why this works**:
1. `(temp - minTemp)`: Shift temperature to start at 0
2. `/ (maxTemp - minTemp)`: Normalize to 0-1 range
3. `× (maxDeg - minDeg)`: Scale to rotation range (270°)
4. `+ minDeg`: Shift to final range (-135° to 135°)

---

## Implementing the Gauge Component

### Component TypeScript (Logic)

Edit `components/temperature-gauge/temperature-gauge.component.ts`:

```typescript
import { Component, Input } from '@angular/core';

@Component({
    selector: 'app-temperature-gauge',
    templateUrl: './temperature-gauge.component.html',
    styleUrls: ['./temperature-gauge.component.scss']
})
export class TemperatureGaugeComponent {
    // Input from parent component
    @Input() temperature: number = 0;
    @Input() isAvailable: boolean = false;
    
    /**
     * Calculate needle rotation based on temperature
     * 
     * @returns CSS transform string
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
     * Get color based on temperature range
     * 
     * @returns CSS color string
     */
    getTemperatureColor(): string {
        if (!this.isAvailable) {
            return '#9e9e9e';  // Grey when offline
        }
        
        // Color scale based on temperature
        if (this.temperature < 0) return '#2196f3';   // Blue (freezing)
        if (this.temperature < 20) return '#03a9f4';  // Light blue (cold)
        if (this.temperature < 30) return '#4caf50';  // Green (comfortable)
        if (this.temperature < 40) return '#ff9800';  // Orange (warm)
        return '#f44336';  // Red (hot)
    }
    
    /**
     * Format temperature for display
     * 
     * @returns Formatted temperature string
     */
    getFormattedTemperature(): string {
        if (!this.isAvailable) {
            return '--';
        }
        return this.temperature.toFixed(1);
    }
}
```

**Key concepts**:

1. **@Input() decorator**: Receive data from parent
   ```typescript
   @Input() temperature: number = 0;
   // Parent can set: <app-temperature-gauge [temperature]="25.5">
   ```

2. **Methods for template**: Return values for HTML
   ```typescript
   getGaugeRotation(): string {
       return `rotate(${angle}deg)`;
   }
   // Used in template: [style.transform]="getGaugeRotation()"
   ```

3. **Clamping values**: Ensure within range
   ```typescript
   const clamped = Math.max(min, Math.min(max, value));
   // If value < min → min
   // If value > max → max
   // Otherwise → value
   ```

### Component Template (HTML)

Edit `components/temperature-gauge/temperature-gauge.component.html`:

```html
<div class="gauge-container">
    <div class="gauge">
        <!-- Gauge body with background -->
        <div class="gauge-body">
            
            <!-- Temperature marks around the edge -->
            <div class="gauge-marks">
                <div class="mark" style="transform: rotate(-135deg)">
                    <span>-40°</span>
                </div>
                <div class="mark" style="transform: rotate(-90deg)">
                    <span>0°</span>
                </div>
                <div class="mark" style="transform: rotate(-45deg)">
                    <span>25°</span>
                </div>
                <div class="mark" style="transform: rotate(0deg)">
                    <span>50°</span>
                </div>
                <div class="mark" style="transform: rotate(45deg)">
                    <span>75°</span>
                </div>
                <div class="mark" style="transform: rotate(90deg)">
                    <span>100°</span>
                </div>
                <div class="mark" style="transform: rotate(135deg)">
                    <span>125°</span>
                </div>
            </div>
            
            <!-- Needle that rotates based on temperature -->
            <div class="needle" 
                 [style.transform]="getGaugeRotation()"
                 [style.background]="getTemperatureColor()">
            </div>
            
            <!-- Center pivot point -->
            <div class="needle-center"></div>
        </div>
        
        <!-- Digital display below gauge -->
        <div class="gauge-display">
            <div class="temperature" [style.color]="getTemperatureColor()">
                {{ getFormattedTemperature() }}<span class="unit">°C</span>
            </div>
            <div class="status" [class.offline]="!isAvailable">
                {{ isAvailable ? 'Online' : 'Offline' }}
            </div>
        </div>
    </div>
</div>
```

**Angular template syntax explained**:

1. **Property binding** `[property]="expression"`:
   ```html
   <div [style.transform]="getGaugeRotation()">
   <!-- Binds style.transform to method result -->
   ```

2. **Interpolation** `{{ expression }}`:
   ```html
   {{ getFormattedTemperature() }}
   <!-- Displays method result as text -->
   ```

3. **Class binding** `[class.className]="boolean"`:
   ```html
   <div [class.offline]="!isAvailable">
   <!-- Adds 'offline' class if isAvailable is false -->
   ```

4. **Conditional (ternary) operator**:
   ```html
   {{ isAvailable ? 'Online' : 'Offline' }}
   <!-- If isAvailable true → "Online", else → "Offline" -->
   ```

### Component Styles (SCSS)

Edit `components/temperature-gauge/temperature-gauge.component.scss`:

```scss
// Container centers the gauge
.gauge-container {
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
}

// Main gauge wrapper
.gauge {
    position: relative;
    width: 300px;
    height: 300px;
}

// Circular gauge body
.gauge-body {
    position: relative;
    width: 100%;
    height: 100%;
    border-radius: 50%;  // Makes it circular
    background: linear-gradient(135deg, #f5f5f5 0%, #e0e0e0 100%);
    box-shadow: 
        0 10px 30px rgba(0, 0, 0, 0.1),           // Outer shadow (depth)
        inset 0 2px 5px rgba(255, 255, 255, 0.5); // Inner highlight (3D effect)
    overflow: hidden;
}

// Temperature marks container
.gauge-marks {
    position: absolute;
    width: 100%;
    height: 100%;
    
    // Individual mark
    .mark {
        position: absolute;
        width: 2px;
        height: 20px;
        background: #666;
        left: 50%;              // Center horizontally
        top: 10px;              // Position from top
        margin-left: -1px;      // Center the 2px line
        transform-origin: center 140px;  // Pivot point (center of circle)
        
        // Temperature label
        span {
            position: absolute;
            top: 25px;          // Below the mark line
            left: 50%;
            transform: translateX(-50%);  // Center the text
            font-size: 11px;
            color: #666;
            white-space: nowrap;
            font-weight: 500;
        }
    }
}

// Needle that points to temperature
.needle {
    position: absolute;
    width: 4px;
    height: 120px;
    background: #f44336;  // Default red (overridden by component)
    left: 50%;
    bottom: 50%;          // Start from center
    margin-left: -2px;    // Center the 4px needle
    transform-origin: center bottom;  // Rotate around bottom center
    border-radius: 2px 2px 0 0;      // Rounded top
    transition: transform 0.5s ease-out;  // Smooth rotation animation
    box-shadow: 0 0 10px rgba(0, 0, 0, 0.3);
    z-index: 2;  // Above marks, below center
}

// Center pivot point
.needle-center {
    position: absolute;
    width: 20px;
    height: 20px;
    background: #333;
    border-radius: 50%;  // Circular
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%);  // Perfect centering
    box-shadow: 0 2px 5px rgba(0, 0, 0, 0.3);
    z-index: 3;  // Above needle
}

// Digital display below gauge
.gauge-display {
    position: absolute;
    bottom: 60px;
    left: 50%;
    transform: translateX(-50%);
    text-align: center;
    
    // Temperature value
    .temperature {
        font-size: 32px;
        font-weight: 700;
        line-height: 1;
        
        // Degree symbol and unit
        .unit {
            font-size: 18px;
            font-weight: 400;
        }
    }
    
    // Status text (Online/Offline)
    .status {
        font-size: 12px;
        color: #4caf50;  // Green for online
        font-weight: 500;
        margin-top: 5px;
        text-transform: uppercase;
        letter-spacing: 0.5px;
        
        // Red when offline
        &.offline {
            color: #f44336;
        }
    }
}
```

**CSS concepts explained**:

1. **Flexbox centering**:
   ```scss
   display: flex;
   justify-content: center;  // Horizontal center
   align-items: center;      // Vertical center
   ```

2. **Circular shape**:
   ```scss
   border-radius: 50%;  // Makes square into circle
   ```

3. **Position: absolute**:
   ```scss
   position: absolute;
   left: 50%;
   top: 50%;
   transform: translate(-50%, -50%);
   // Centers element regardless of size
   ```

4. **Transform-origin**:
   ```scss
   transform-origin: center bottom;  // Rotation pivot point
   transform: rotate(45deg);         // Rotates around bottom-center
   ```

5. **Box-shadow** (3D effect):
   ```scss
   box-shadow: 
       0 10px 30px rgba(0,0,0,0.1),           // Outer shadow
       inset 0 2px 5px rgba(255,255,255,0.5); // Inner highlight
   ```

6. **CSS transitions** (animations):
   ```scss
   transition: transform 0.5s ease-out;
   // Property Duration Timing
   // Smoothly animates transform changes over 0.5s
   ```

7. **Parent selector** (`&`):
   ```scss
   .status {
       color: green;
       
       &.offline {  // Same as .status.offline
           color: red;
       }
   }
   ```

---

# Section F: Real-time Chart with Chart.js

## Understanding Chart.js

**Chart.js** = JavaScript charting library

**Why Chart.js?**
- Simple API
- Responsive (adapts to screen size)
- Interactive (tooltips, animations)
- Many chart types (line, bar, pie, etc.)

### Installation

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Install Chart.js and Angular wrapper
npm install chart.js ng2-charts

# Install type definitions
npm install --save-dev @types/chart.js
```

**What we installed**:
- `chart.js`: Core charting library
- `ng2-charts`: Angular wrapper (makes it work with Angular)
- `@types/chart.js`: TypeScript type definitions

### Register in Angular Module

Edit `app.module.ts`:

```typescript
import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { HttpClientModule } from '@angular/common/http';
import { NgChartsModule } from 'ng2-charts';  // ← Add this

import { AppComponent } from './app.component';

@NgModule({
    declarations: [
        AppComponent,
        // ... your components
    ],
    imports: [
        BrowserModule,
        HttpClientModule,
        NgChartsModule,  // ← Add this
    ],
    providers: [],
    bootstrap: [AppComponent]
})
export class AppModule { }
```

---

## Creating Temperature Chart Component

### Generate Component

```bash
ng generate component components/temperature-chart
```

### Component TypeScript

Edit `components/temperature-chart/temperature-chart.component.ts`:

```typescript
import { Component, OnInit, OnDestroy } from '@angular/core';
import { Subscription, interval } from 'rxjs';
import { switchMap } from 'rxjs/operators';
import { ChartConfiguration, ChartOptions, ChartType } from 'chart.js';
import { TemperatureService } from '../../services/temperature.service';

@Component({
    selector: 'app-temperature-chart',
    templateUrl: './temperature-chart.component.html',
    styleUrls: ['./temperature-chart.component.scss']
})
export class TemperatureChartComponent implements OnInit, OnDestroy {
    private subscription?: Subscription;

    // Chart data structure
    public lineChartData: ChartConfiguration<'line'>['data'] = {
        labels: [],  // X-axis labels (timestamps)
        datasets: [
            {
                data: [],  // Y-axis data (temperatures)
                label: 'Temperature (°C)',
                fill: true,  // Fill area under line
                tension: 0.4,  // Curve smoothness (0 = straight, 1 = very curved)
                borderColor: '#2196f3',  // Line color
                backgroundColor: 'rgba(33, 150, 243, 0.1)',  // Fill color (transparent blue)
                pointBackgroundColor: '#2196f3',  // Point color
                pointBorderColor: '#fff',  // Point outline
                pointHoverBackgroundColor: '#fff',  // Point hover color
                pointHoverBorderColor: '#2196f3',  // Point hover outline
                pointRadius: 3,  // Point size
                pointHoverRadius: 5,  // Point size on hover
            }
        ]
    };

    // Chart configuration
    public lineChartOptions: ChartOptions<'line'> = {
        responsive: true,  // Resize with container
        maintainAspectRatio: false,  // Allow custom height
        
        // Plugins configuration
        plugins: {
            legend: {
                display: true,  // Show legend
                position: 'top',
            },
            tooltip: {
                enabled: true,  // Show tooltips on hover
                mode: 'index',  // Show all datasets at same X position
                intersect: false,  // Don't require exact point hover
            }
        },
        
        // Axes configuration
        scales: {
            y: {
                title: {
                    display: true,
                    text: 'Temperature (°C)'
                },
                beginAtZero: false,  // Don't force Y-axis to start at 0
                ticks: {
                    callback: function(value) {
                        return value + '°C';  // Add unit to Y-axis labels
                    }
                }
            },
            x: {
                title: {
                    display: true,
                    text: 'Time'
                },
                ticks: {
                    maxRotation: 45,  // Rotate labels if crowded
                    minRotation: 0
                }
            }
        },
        
        // Interaction options
        interaction: {
            mode: 'nearest',
            axis: 'x',
            intersect: false
        }
    };

    public lineChartType: ChartType = 'line';

    constructor(private temperatureService: TemperatureService) { }

    ngOnInit(): void {
        // Update chart every 2 seconds
        this.subscription = interval(2000).pipe(
            switchMap(() => this.temperatureService.getHistory())
        ).subscribe(data => {
            this.updateChart(data.history);
        });
    }

    ngOnDestroy(): void {
        this.subscription?.unsubscribe();
    }

    /**
     * Update chart with new data
     * 
     * @param temperatures Array of temperature readings
     */
    private updateChart(temperatures: number[]): void {
        const now = new Date();
        
        // Generate labels (timestamps for each reading)
        // Assuming readings are 2s apart, work backwards from now
        const labels = temperatures.map((_, index) => {
            const time = new Date(now.getTime() - (temperatures.length - index - 1) * 2000);
            return time.toLocaleTimeString('en-US', { 
                hour: '2-digit', 
                minute: '2-digit',
                second: '2-digit'
            });
        });
        
        // Update chart data
        this.lineChartData.labels = labels;
        this.lineChartData.datasets[0].data = temperatures;
        
        // Note: Angular's change detection automatically updates chart
    }
}
```

**Key concepts**:

1. **ChartConfiguration type**: TypeScript type safety for chart config
   ```typescript
   public lineChartData: ChartConfiguration<'line'>['data'] = { ... };
   // 'line' = chart type
   // ['data'] = extract data property type
   ```

2. **Chart dataset structure**:
   ```typescript
   datasets: [
       {
           data: [25.5, 25.3, 25.7],     // Y values
           label: 'Temperature',          // Legend label
           borderColor: '#2196f3',        // Line color
           backgroundColor: 'rgba(...)',  // Fill color
       }
   ]
   ```

3. **Responsive configuration**:
   ```typescript
   responsive: true,              // Resize with container
   maintainAspectRatio: false,   // Custom height possible
   ```

4. **Callback for formatting**:
   ```typescript
   ticks: {
       callback: function(value) {
           return value + '°C';  // Add unit
       }
   }
   ```

5. **Array.map()** for transforming data:
   ```typescript
   const labels = temperatures.map((temp, index) => {
       // Create label for each temperature
       return formatTime(index);
   });
   ```

### Component Template

Edit `components/temperature-chart/temperature-chart.component.html`:

```html
<div class="chart-container">
    <canvas baseChart
            [data]="lineChartData"
            [options]="lineChartOptions"
            [type]="lineChartType">
    </canvas>
</div>
```

**Explanation**:

1. **baseChart directive**: Provided by ng2-charts
   ```html
   <canvas baseChart>
   <!-- Turns canvas into Chart.js chart -->
   ```

2. **Property bindings**: Pass configuration to chart
   ```html
   [data]="lineChartData"      <!-- Chart data -->
   [options]="lineChartOptions" <!-- Chart options -->
   [type]="lineChartType"       <!-- Chart type -->
   ```

3. **Why canvas?**: Chart.js draws on HTML5 canvas element
   - High performance
   - Pixel-level control
   - Hardware accelerated

### Component Styles

Edit `components/temperature-chart/temperature-chart.component.scss`:

```scss
.chart-container {
    position: relative;
    height: 300px;  // Fixed height
    width: 100%;
    padding: 20px;
    
    // Ensure canvas scales properly
    canvas {
        max-width: 100%;
    }
}
```

**Why fixed height?**
- Chart.js needs explicit height to render
- `maintainAspectRatio: false` allows custom height
- Responsive width via `width: 100%`

---

# Section G: Material Design and UX

## Angular Material

**Angular Material** = Google's Material Design for Angular

**Benefits**:
- Professional appearance
- Accessibility built-in
- Consistent with Android/Google apps
- Theming support

### Installation

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Add Angular Material
ng add @angular/material

# Answer prompts:
# ? Choose a prebuilt theme: Indigo/Pink
# ? Set up global Angular Material typography? Yes
# ? Include Angular animations module? Yes
```

**What this does**:
- Installs @angular/material
- Installs @angular/cdk (Component Dev Kit)
- Sets up themes
- Configures animations
- Updates angular.json and styles

### Import Material Modules

Create `src/app/material.module.ts`:

```typescript
import { NgModule } from '@angular/core';

// Import Material modules we'll use
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatCardModule } from '@angular/material/card';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatSnackBarModule } from '@angular/material/snack-bar';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';

// Array of modules
const materialModules = [
    MatToolbarModule,
    MatCardModule,
    MatButtonModule,
    MatIconModule,
    MatSnackBarModule,
    MatProgressSpinnerModule,
];

/**
 * Material Module - imports and exports all Material components
 */
@NgModule({
    imports: materialModules,
    exports: materialModules
})
export class MaterialModule { }
```

**Why separate module?**
- Organize Material imports
- Reusable across app
- Easy to add/remove modules

Then import in `app.module.ts`:

```typescript
import { MaterialModule } from './material.module';

@NgModule({
    imports: [
        BrowserModule,
        HttpClientModule,
        NgChartsModule,
        MaterialModule,  // ← Add this
    ],
})
export class AppModule { }
```

---

## Creating Control Panel Component

### Generate Component

```bash
ng generate component components/control-panel
```

### Component TypeScript

Edit `components/control-panel/control-panel.component.ts`:

```typescript
import { Component, Output, EventEmitter } from '@angular/core';
import { MatSnackBar } from '@angular/material/snack-bar';

@Component({
    selector: 'app-control-panel',
    templateUrl: './control-panel.component.html',
    styleUrls: ['./control-panel.component.scss']
})
export class ControlPanelComponent {
    // Events sent to parent component
    @Output() enableClicked = new EventEmitter<void>();
    @Output() disableClicked = new EventEmitter<void>();
    @Output() resetClicked = new EventEmitter<void>();
    
    /**
     * Constructor - Angular injects MatSnackBar service
     */
    constructor(private snackBar: MatSnackBar) { }
    
    /**
     * Handle Enable button click
     */
    onEnable(): void {
        // Emit event to parent
        this.enableClicked.emit();
        
        // Show notification
        this.snackBar.open('Monitoring enabled', 'Close', {
            duration: 2000,  // Auto-dismiss after 2s
            horizontalPosition: 'center',
            verticalPosition: 'bottom',
        });
    }
    
    /**
     * Handle Disable button click
     */
    onDisable(): void {
        this.disableClicked.emit();
        
        this.snackBar.open('Monitoring disabled', 'Close', {
            duration: 2000,
        });
    }
    
    /**
     * Handle Reset button click
     */
    onReset(): void {
        this.resetClicked.emit();
        
        this.snackBar.open('Statistics reset', 'Close', {
            duration: 2000,
        });
    }
}
```

**Key concepts**:

1. **@Output() EventEmitter**: Send events to parent
   ```typescript
   @Output() enableClicked = new EventEmitter<void>();
   
   // Emit event
   this.enableClicked.emit();
   
   // Parent listens:
   // <app-control-panel (enableClicked)="handleEnable()">
   ```

2. **MatSnackBar**: Show toast notifications
   ```typescript
   constructor(private snackBar: MatSnackBar) { }
   
   this.snackBar.open('Message', 'Action', {
       duration: 2000,  // ms
   });
   ```

3. **Generic types** (`<void>`):
   ```typescript
   EventEmitter<void>    // No data
   EventEmitter<string>  // Emits string
   EventEmitter<number>  // Emits number
   ```

### Component Template

Edit `components/control-panel/control-panel.component.html`:

```html
<mat-card>
    <mat-card-header>
        <mat-card-title>Controls</mat-card-title>
    </mat-card-header>
    
    <mat-card-content>
        <div class="button-row">
            <!-- Enable button -->
            <button mat-raised-button 
                    color="primary" 
                    (click)="onEnable()">
                <mat-icon>play_arrow</mat-icon>
                Enable
            </button>
            
            <!-- Disable button -->
            <button mat-raised-button 
                    color="warn" 
                    (click)="onDisable()">
                <mat-icon>pause</mat-icon>
                Disable
            </button>
            
            <!-- Reset button -->
            <button mat-raised-button 
                    (click)="onReset()">
                <mat-icon>refresh</mat-icon>
                Reset Stats
            </button>
        </div>
    </mat-card-content>
</mat-card>
```

**Material components**:

1. **mat-card**: Container with elevation
   ```html
   <mat-card>
       <mat-card-header>...</mat-card-header>
       <mat-card-content>...</mat-card-content>
   </mat-card>
   ```

2. **mat-raised-button**: Button with elevation
   ```html
   <button mat-raised-button color="primary">
   <!-- color: primary, accent, warn -->
   ```

3. **mat-icon**: Material Design icons
   ```html
   <mat-icon>play_arrow</mat-icon>
   <!-- Icon name from: https://fonts.google.com/icons -->
   ```

### Component Styles

Edit `components/control-panel/control-panel.component.scss`:

```scss
.button-row {
    display: flex;
    gap: 10px;  // Space between buttons
    flex-wrap: wrap;  // Wrap on small screens
    
    button {
        flex: 1;  // Equal width buttons
        min-width: 120px;  // Minimum button width
        
        // Icon inside button
        mat-icon {
            margin-right: 8px;  // Space between icon and text
        }
    }
}
```

---

## Main App Component (Putting It All Together)

### App Component TypeScript

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
    temperatureData?: TemperatureData;
    private subscription?: Subscription;

    constructor(private temperatureService: TemperatureService) { }

    ngOnInit(): void {
        // Subscribe to temperature stream
        this.subscription = this.temperatureService
            .getTemperatureStream()
            .subscribe(data => {
                this.temperatureData = data;
            });
    }

    ngOnDestroy(): void {
        this.subscription?.unsubscribe();
    }

    /**
     * Handle enable button click from control panel
     */
    handleEnable(): void {
        console.log('Enable monitoring');
        // TODO: Send enable command to ESP32
    }

    /**
     * Handle disable button click from control panel
     */
    handleDisable(): void {
        console.log('Disable monitoring');
        // TODO: Send disable command to ESP32
    }

    /**
     * Handle reset button click from control panel
     */
    handleReset(): void {
        console.log('Reset statistics');
        // TODO: Send reset command to ESP32
    }
}
```

### App Component Template

Edit `app.component.html`:

```html
<!-- Top toolbar -->
<mat-toolbar color="primary">
    <span>{{ title }}</span>
    <span class="spacer"></span>
    <span *ngIf="temperatureData">
        Reads: {{ temperatureData.readCount }} | 
        Errors: {{ temperatureData.errorCount }}
    </span>
</mat-toolbar>

<!-- Main content -->
<div class="container">
    <!-- Top row: Gauge and controls -->
    <div class="row">
        <!-- Left column: Temperature gauge -->
        <div class="col-md-6">
            <mat-card>
                <mat-card-header>
                    <mat-card-title>Current Temperature</mat-card-title>
                </mat-card-header>
                <mat-card-content>
                    <app-temperature-gauge
                        [temperature]="temperatureData?.temperature || 0"
                        [isAvailable]="temperatureData?.available || false">
                    </app-temperature-gauge>
                </mat-card-content>
            </mat-card>
        </div>
        
        <!-- Right column: Controls and info -->
        <div class="col-md-6">
            <!-- Control panel -->
            <app-control-panel
                (enableClicked)="handleEnable()"
                (disableClicked)="handleDisable()"
                (resetClicked)="handleReset()">
            </app-control-panel>
            
            <!-- System info card -->
            <mat-card class="info-card">
                <mat-card-header>
                    <mat-card-title>System Info</mat-card-title>
                </mat-card-header>
                <mat-card-content *ngIf="temperatureData">
                    <div class="info-row">
                        <span class="label">Status:</span>
                        <span [class.online]="temperatureData.available" 
                              [class.offline]="!temperatureData.available">
                            {{ temperatureData.available ? 'Online' : 'Offline' }}
                        </span>
                    </div>
                    <div class="info-row">
                        <span class="label">Last Update:</span>
                        <span>{{ temperatureData.lastUpdate | date:'medium' }}</span>
                    </div>
                    <div class="info-row">
                        <span class="label">Data Age:</span>
                        <span>{{ temperatureData.dataAge / 1000 | number:'1.0-0' }}s</span>
                    </div>
                </mat-card-content>
            </mat-card>
        </div>
    </div>
    
    <!-- Bottom row: Temperature chart -->
    <div class="row">
        <div class="col-12">
            <mat-card>
                <mat-card-header>
                    <mat-card-title>Temperature History</mat-card-title>
                </mat-card-header>
                <mat-card-content>
                    <app-temperature-chart></app-temperature-chart>
                </mat-card-content>
            </mat-card>
        </div>
    </div>
</div>
```

**Template features**:

1. **Optional chaining** (`?.`):
   ```html
   {{ temperatureData?.temperature }}
   <!-- If temperatureData is undefined → shows nothing (no error) -->
   ```

2. **Nullish coalescing** (`||`):
   ```html
   [temperature]="temperatureData?.temperature || 0"
   <!-- If undefined → use 0 -->
   ```

3. **Structural directive** (`*ngIf`):
   ```html
   <div *ngIf="temperatureData">
   <!-- Only render if temperatureData exists -->
   ```

4. **Pipes** (|):
   ```html
   {{ temperatureData.lastUpdate | date:'medium' }}
   <!-- Formats timestamp as human-readable date -->
   
   {{ temperatureData.dataAge / 1000 | number:'1.0-0' }}
   <!-- Formats number: min 1 integer, 0-0 decimal places -->
   ```

5. **Event binding from child**:
   ```html
   <app-control-panel (enableClicked)="handleEnable()">
   <!-- Listen to child's event, call parent's method -->
   ```

### App Component Styles

Edit `app.component.scss`:

```scss
// Spacer to push content to edges in toolbar
.spacer {
    flex: 1 1 auto;
}

// Main container
.container {
    padding: 20px;
    max-width: 1400px;
    margin: 0 auto;  // Center container
}

// Row with columns
.row {
    display: flex;
    gap: 20px;
    margin-bottom: 20px;
    flex-wrap: wrap;  // Stack on small screens
    
    // Column sizing
    .col-md-6 {
        flex: 1;  // Equal width
        min-width: 300px;  // Minimum before wrapping
    }
    
    .col-12 {
        flex: 1 1 100%;  // Full width
    }
}

// Info card styling
.info-card {
    margin-top: 20px;
}

// Info rows inside card
.info-row {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid #eee;
    
    &:last-child {
        border-bottom: none;
    }
    
    .label {
        font-weight: 500;
        color: #666;
    }
    
    .online {
        color: #4caf50;
        font-weight: 500;
    }
    
    .offline {
        color: #f44336;
        font-weight: 500;
    }
}
```

---

# Section H: Build and Deployment

## Development vs Production

### Development Mode

```bash
npm start
# or
ng serve
```

**Characteristics**:
- Source maps included (debugging)
- Not minified (readable)
- Hot reload (auto-refresh on changes)
- Larger file sizes
- **Port**: http://localhost:4200

### Production Mode

```bash
npm run build
# or
ng build --configuration production
```

**Characteristics**:
- No source maps
- Minified (smaller files)
- Optimized (tree-shaking, dead code elimination)
- AOT compilation (Ahead-of-Time)
- **Output**: `dist/temp-monitor/`

---

## Build Process Explained

### What Happens During Build?

```
1. TypeScript → JavaScript
   ├─ Type checking
   └─ Compilation

2. SCSS → CSS
   ├─ Variable substitution
   └─ Minification

3. Bundling
   ├─ Combine files
   ├─ Tree-shaking (remove unused code)
   └─ Code splitting (separate bundles)

4. Optimization
   ├─ Minification
   ├─ Uglification
   └─ Compression

5. Output
   └─ dist/
       ├─ index.html
       ├─ main.[hash].js
       ├─ polyfills.[hash].js
       └─ styles.[hash].css
```

### Build Output

```bash
ng build --configuration production

# Output files:
dist/temp-monitor/
├── index.html              # Main HTML
├── main.a3f9d2c8.js       # Application code
├── polyfills.4f1c2d3e.js  # Browser compatibility
├── runtime.b7e3c2d1.js    # Webpack runtime
└── styles.9c8a1e4f.css    # Compiled styles
```

**Hash in filenames** (e.g., `a3f9d2c8`):
- Changes when file content changes
- Browser cache busting
- Load latest version automatically

---

## Deploying to ESP32

### Step 1: Build Angular App

```bash
cd ~/Work/temperature-monitor/webapp/temp-monitor

# Production build
ng build --configuration production

# Output is in: dist/temp-monitor/
```

### Step 2: Copy to ESP32 Project

```bash
# Copy built files to ESP32 www directory
cp -r dist/temp-monitor/* ../../www/

# Verify
ls -la ../../www/
# Should show: index.html, *.js, *.css, assets/
```

### Step 3: Configure ESP32 to Serve Files

We need SPIFFS (SPI Flash File System) to store web files.

**Create partition table** (`partitions.csv` in ESP32 project root):

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 2M,
www,      data, spiffs,  ,        2M,
```

**Update sdkconfig.defaults**:

```ini
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

### Step 4: Add SPIFFS Support to ESP32

**Update `main/CMakeLists.txt`**:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        "driver"
        "esp_wifi"
        "esp_http_server"
        "nvs_flash"
        "esp_netif"
        "mcp9808"
        "esp_timer"
        "spiffs"  # ← Add this
)
```

**Add SPIFFS initialization in `main.c`**:

```c
#include "esp_spiffs.h"

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/www",              // Mount point
        .partition_label = "www",         // Partition name
        .max_files = 10,                  // Max open files
        .format_if_mount_failed = true    // Format if needed
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }
    
    // Check partition info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("www", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }
}

// In app_main():
void app_main(void) {
    // ... other initialization ...
    
    init_spiffs();  // Initialize SPIFFS
    
    // ... rest of code ...
}
```

**Add file serving handler**:

```c
static esp_err_t serve_static_file(httpd_req_t *req) {
    char filepath[256];
    
    // Map URL to file path
    if (strcmp(req->uri, "/") == 0) {
        strcpy(filepath, "/www/index.html");
    } else {
        snprintf(filepath, sizeof(filepath), "/www%s", req->uri);
    }
    
    // Open file
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Determine content type
    const char *content_type = "text/html";
    if (strstr(filepath, ".js")) {
        content_type = "application/javascript";
    } else if (strstr(filepath, ".css")) {
        content_type = "text/css";
    } else if (strstr(filepath, ".json")) {
        content_type = "application/json";
    }
    
    httpd_resp_set_type(req, content_type);
    
    // Send file in chunks
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            fclose(file);
            return ESP_FAIL;
        }
    }
    
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);  // End of chunks
    return ESP_OK;
}

// Register wildcard handler (catches all routes)
httpd_uri_t static_files = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = serve_static_file,
};
httpd_register_uri_handler(server, &static_files);
```

### Step 5: Flash SPIFFS Partition

```bash
cd ~/Work/temperature-monitor

# Create SPIFFS image from www/ directory
python $IDF_PATH/components/spiffs/spiffsgen.py 2097152 www spiffs.bin

# Flash SPIFFS partition
esptool.py --chip esp32s3 --port /dev/cu.usbmodem141201 \
    write_flash 0x310000 spiffs.bin

# (0x310000 = offset from partition table)
```

**Or easier - flash everything**:

```bash
idf.py build
idf.py -p /dev/cu.usbmodem141201 flash
```

### Step 6: Test Complete System

1. **Flash ESP32**: `idf.py flash monitor`
2. **Connect to WiFi**: ESP32-Temperature
3. **Open browser**: http://192.168.4.1
4. **See**: Full Angular app!

---

## Troubleshooting

### Angular Build Issues

**Error: "Cannot find module"**:
```bash
rm -rf node_modules package-lock.json
npm install
```

**TypeScript errors**:
```bash
# Check tsconfig.json
# Ensure "strict": false for easier development
```

### ESP32 Issues

**SPIFFS mount failed**:
```
E (1234) spiffs: Failed to initialize SPIFFS (ESP_ERR_NOT_FOUND)
```
**Solution**: Check partition table, flash SPIFFS partition

**404 errors**:
```
GET / → 404 Not Found
```
**Solution**: 
- Verify files in /www directory on ESP32
- Check wildcard handler registered
- Check file paths match

**Out of memory**:
```
E (1234) httpd: httpd_server: Failed to allocate memory
```
**Solution**:
- Reduce www partition size
- Enable PSRAM in menuconfig
- Reduce max_open_sockets in httpd config

---

## Performance Optimization

### 1. Enable Compression

ESP32 can serve compressed files:

```c
// In serve_static_file():
// Check for .gz version first
char gzpath[260];
snprintf(gzpath, sizeof(gzpath), "%s.gz", filepath);

FILE *file = fopen(gzpath, "r");
if (file != NULL) {
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
} else {
    file = fopen(filepath, "r");
}
```

**Pre-compress files**:
```bash
cd www
gzip -k *.js *.css
# Creates *.js.gz, *.css.gz
```

### 2. Enable Caching

Add cache headers:

```c
// Cache static files for 1 hour
httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
```

### 3. Lazy Loading (Angular)

Split app into modules that load on-demand:

```typescript
// app-routing.module.ts
const routes: Routes = [
    {
        path: 'admin',
        loadChildren: () => import('./admin/admin.module')
            .then(m => m.AdminModule)
    }
];
```

---

## Final Questions

1. **Why use services instead of putting logic in components?**
   <details>
   <summary>Answer</summary>
   - Separation of concerns (UI vs business logic)
   - Reusability across multiple components
   - Easier testing (mock services)
   - Singleton pattern (shared state)
   - Cleaner component code
   </details>

2. **What's the difference between `ngOnInit` and constructor?**
   <details>
   <summary>Answer</summary>
   - Constructor: Dependency injection only, component not initialized
   - ngOnInit: Component initialized, inputs available, safe to load data
   - Rule: Use constructor for DI, ngOnInit for logic
   </details>

3. **Why must we unsubscribe in `ngOnDestroy`?**
   <details>
   <summary>Answer</summary>
   - Prevent memory leaks (subscription keeps running)
   - Component reference not garbage collected
   - Multiple subscriptions if component recreated
   - Can cause crashes with enough leaks
   </details>

4. **How does Angular know when to update the view?**
   <details>
   <summary>Answer</summary>
   - Zone.js intercepts async operations
   - Triggers change detection after:
     - Events (clicks, inputs)
     - Timers (setTimeout, setInterval)
     - HTTP requests
   - Checks component tree for changes
   - Updates DOM if data changed
   </details>

5. **Why hash filenames in production build?**
   <details>
   <summary>Answer</summary>
   - Cache busting: New hash = new file
   - Browser loads latest version automatically
   - Can cache files forever (hash changes = new file)
   - No need to clear browser cache
   </details>

---

## Complete System Summary

**What we built**:

```
ESP32 Hardware
    ↓
I2C Driver (MCP9808)
    ↓
FreeRTOS Task (reads sensor)
    ↓
HTTP Server (serves API + files)
    ↓
Angular App (TypeScript + HTML + SCSS)
    ├─ Temperature Service (HTTP calls)
    ├─ Gauge Component (visual display)
    ├─ Chart Component (history graph)
    └─ Control Panel (user controls)
```

**Technologies mastered**:
- ESP32 & ESP-IDF
- I2C protocol
- FreeRTOS
- HTTP server
- TypeScript
- Angular framework
- RxJS
- Chart.js
- Material Design
- SCSS styling
- Build optimization
- SPIFFS file system

---

## Next Steps

1. **Add more sensors**: Pressure, humidity, etc.
2. **Data logging**: Store to SD card
3. **Cloud integration**: Send data to server
4. **Authentication**: Secure the interface
5. **WebSocket**: Real-time updates (no polling)
6. **Mobile app**: Ionic framework (uses Angular)
7. **Alerts**: Email/SMS notifications
8. **Configuration UI**: Set thresholds, WiFi settings

---

**Congratulations!** You've built a complete IoT system from hardware to web interface. You now understand:
- Embedded systems programming
- Web application development
- Real-time data visualization
- Full-stack architecture

This knowledge applies to countless IoT projects! 🎉
