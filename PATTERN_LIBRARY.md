# Conway's Game of Life Pattern Library Guide

This guide explains how to find, create, load, and share patterns for the advanced Game of Life simulator.

## 🎨 What are Patterns?

Patterns are specific initial configurations of cells that exhibit interesting behavior:

- **Still lifes**: Never change (Block, Beehive, Loaf)
- **Oscillators**: Repeat after N generations (Blinker, Toad, Pulsar)
- **Spaceships**: Move across the grid (Glider, LWSS, MWSS)
- **Methuselahs**: Small patterns that evolve for many generations
- **Puffer trains**: Leave debris behind while moving
- **Guns**: Emit spaceships periodically (Gosper Glider Gun)
- **Infinite growth**: Pattern that grows without bound

---

## 📥 Loading Patterns

### Built-in Patterns

The advanced version includes three famous patterns:

```powershell
# Run advanced version
.\build_advanced\gameoflife_advanced.exe

# Press keys to load:
g   → Glider (smallest spaceship)
b   → Blinker (period-2 oscillator)
G   → Gosper Glider Gun (first discovered gun)
```

### RLE Files

**RLE** (Run-Length Encoded) is the standard format for Conway's Game of Life patterns.

**Loading from file**:
```powershell
# Place .rle file in same directory as exe
# Run with -p flag
.\gameoflife_advanced.exe -p glider_gun.rle

# Or press 'L' during runtime and select file
```

---

## 📝 RLE Format Specification

### Basic Structure

```
#C This is a comment
#C Author: John Conway
#C Description: Gosper Glider Gun
x = 36, y = 9, rule = B3/S23
24bo$22bobo$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o$2o8bo3bob2o4b
obo$10bo5bo7bo$11bo3bo$12b2o!
```

### Header Lines
- `#C` - Comment
- `#N` - Name (optional)
- `#O` - Author (optional)
- `x = W, y = H, rule = RULE` - Dimensions and rule

### Encoding Rules
- `b` - Dead cell
- `o` - Live cell
- `<number><char>` - Repeat character N times (e.g., `3o` = `ooo`)
- `$` - End of line
- `!` - End of pattern

### Examples

**Glider**:
```
#N Glider
#C Smallest spaceship
x = 3, y = 3, rule = B3/S23
bob$2bo$3o!
```

Decoded:
```
.O.
..O
OOO
```

**Blinker**:
```
#N Blinker
#C Period 2 oscillator
x = 3, y = 1, rule = B3/S23
3o!
```

Decoded:
```
OOO
```

**Glider with run-length encoding**:
```
x = 3, y = 3
bo$2bo$3o!

# is same as:
b1o1$
2b1o1$
3o!
```

---

## 🌐 Where to Find Patterns

### Online Repositories

1. **LifeWiki** (best source)
   - URL: https://conwaylife.com/wiki/
   - Content: 1000+ patterns with descriptions
   - Format: RLE, animations, analysis
   - Download: Click pattern → "Download RLE"

2. **The Life Lexicon**
   - URL: https://conwaylife.com/ref/lexicon/lex.htm
   - Content: Alphabetical list with RLE
   - Good for: Classic patterns

3. **GitHub Pattern Collections**
   - Search: "game of life patterns RLE"
   - Many curated collections

### Recommended Starting Patterns

#### **Oscillators**:
- Blinker (period 2)
- Toad (period 2)
- Beacon (period 2)
- Pulsar (period 3)
- Pentadecathlon (period 15)

#### **Spaceships**:
- Glider (c/4 diagonal)
- Lightweight spaceship (LWSS) (c/2 orthogonal)
- Middleweight spaceship (MWSS)
- Heavyweight spaceship (HWSS)

#### **Guns**:
- Gosper Glider Gun (period 30)
- Simkin Glider Gun (period 120)

#### **Methuselahs**:
- R-pentomino (1103 generations)
- Acorn (5206 generations)
- Lidka (29055 generations)

---

## ✍️ Creating Your Own Patterns

### Method 1: Mouse Editing

```powershell
# Run advanced version
.\gameoflife_advanced.exe

# Use mouse:
Left-click → Toggle cell
Ctrl + Mouse wheel → Zoom in/out
Ctrl + Drag → Pan view

# Edit cells, then:
p → Save screenshot
```

### Method 2: Write RLE by Hand

```
# 1. Plan your pattern on paper
# Example: 3x3 block
OOO
OOO
OOO

# 2. Convert to RLE
x = 3, y = 3
3o$3o$3o!

# 3. Save as pattern.rle
```

### Method 3: Use Pattern Editor

Many online editors available:
- https://copy.sh/life/ (click cells, export RLE)
- https://lazyslug.com/lifeviewer/ (advanced editor)

---

## 🔄 Converting Other Formats

### From Plaintext (.cells)

**Input** (`glider.cells`):
```
!Name: Glider
.O.
..O
OOO
```

**Convert to RLE**:
```python
# Python script
def cells_to_rle(cells_file):
    with open(cells_file) as f:
        lines = [l.strip() for l in f if not l.startswith('!')]
    
    height = len(lines)
    width = max(len(l) for l in lines)
    
    rle = f"x = {width}, y = {height}\n"
    
    for line in lines:
        line = line.ljust(width, '.')  # Pad
        rle += encode_line(line) + "$"
    
    rle = rle.rstrip('$') + '!'
    return rle

def encode_line(line):
    result = ""
    count = 1
    prev = line[0]
    
    for char in line[1:]:
        if char == prev:
            count += 1
        else:
            if count > 1:
                result += str(count)
            result += 'b' if prev == '.' else 'o'
            prev = char
            count = 1
    
    if count > 1:
        result += str(count)
    result += 'b' if prev == '.' else 'o'
    
    return result.rstrip('b')  # Remove trailing dead cells

# Usage
print(cells_to_rle('glider.cells'))
```

**Output**:
```
x = 3, y = 3
bob$2bo$3o!
```

---

## 📦 Pattern Collections

### Create a Pattern Pack

**Directory structure**:
```
patterns/
├── oscillators/
│   ├── blinker.rle
│   ├── toad.rle
│   └── pulsar.rle
├── spaceships/
│   ├── glider.rle
│   ├── lwss.rle
│   └── mwss.rle
└── guns/
    └── gosper_gun.rle
```

**Load patterns**:
```powershell
# From advanced version
L → Opens file dialog → Navigate to pattern → Load
```

---

## 🧪 Testing Patterns

### Verify Behavior

```powershell
# Load pattern
.\gameoflife_advanced.exe -p pattern.rle

# Observe:
SPACE → Pause/Resume
s     → Single step
r     → Reset to initial state

# Check:
- Does it oscillate with expected period?
- Does it move at expected speed?
- Is it stable?
```

### Measure Properties

```powershell
# Run with CSV logging
.\gameoflife_advanced.exe -p pattern.rle -g 1000 -l metrics.csv

# Open metrics.csv
# Check "alive_cells" column for stability
```

**Example analysis**:
```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('metrics.csv')

# Plot population over time
plt.plot(df['generation'], df['alive_cells'])
plt.xlabel('Generation')
plt.ylabel('Alive Cells')
plt.title('Pattern Evolution')
plt.show()

# Detect period
from scipy.signal import find_peaks
peaks, _ = find_peaks(df['alive_cells'])
if len(peaks) > 1:
    period = peaks[1] - peaks[0]
    print(f"Period: {period}")
```

---

## 🎓 Pattern Analysis

### Classify Pattern Type

**Still Life Detection**:
```python
# If population constant for 100 gens → still life
df['alive_cells'].iloc[-100:].nunique() == 1
```

**Oscillator Period**:
```python
# Find period using autocorrelation
from scipy import signal
acf = signal.correlate(df['alive_cells'], df['alive_cells'])
peaks = signal.find_peaks(acf)[0]
period = peaks[1] - peaks[0] if len(peaks) > 1 else None
```

**Spaceship Speed**:
```python
# Record bounding box center over time
# Speed = distance / time
```

---

## 📐 Pattern Transformation

### Rotate

```python
def rotate_rle_90(rle):
    """Rotate pattern 90 degrees clockwise"""
    # Parse RLE
    grid = parse_rle(rle)
    
    # Rotate grid
    rotated = [[grid[y][x] for y in range(len(grid)-1, -1, -1)] 
               for x in range(len(grid[0]))]
    
    # Encode back to RLE
    return grid_to_rle(rotated)
```

### Flip

```python
def flip_horizontal(rle):
    grid = parse_rle(rle)
    flipped = [row[::-1] for row in grid]
    return grid_to_rle(flipped)

def flip_vertical(rle):
    grid = parse_rle(rle)
    flipped = grid[::-1]
    return grid_to_rle(flipped)
```

---

## 🏆 Advanced Pattern Engineering

### Design Custom Oscillators

**Requirements**:
- Period > 2
- Minimal bounding box
- Symmetrical (optional)

**Process**:
1. Start with known oscillator
2. Add/remove cells
3. Test stability for 100+ generations
4. Refine until desired period achieved

### Design Spaceships

**Approach 1: Manual Search**
1. Create small asymmetric pattern
2. Run for many generations
3. Check if it moves
4. Adjust until stable movement

**Approach 2: Use Gfind/Afind**
- Command-line tools for exhaustive search
- Can find new spaceships automatically

### Design Guns

**Method**: Combine oscillators and eaters
1. Place oscillator that emits gliders
2. Add "eaters" to destroy unwanted gliders
3. Tune timing for desired period

---

## 🔀 Rule Variants

The advanced version supports different Life-like rules:

### Available Rules

1. **Conway's Life** (B3/S23) - Default
   - Born: 3 neighbors
   - Survive: 2 or 3 neighbors

2. **HighLife** (B36/S23)
   - Born: 3 or 6 neighbors
   - Survive: 2 or 3 neighbors
   - Has a small replicator!

3. **Seeds** (B2/S)
   - Born: 2 neighbors
   - Survive: never
   - Everything dies each generation

4. **Day & Night** (B3678/S34678)
   - Symmetric rule
   - Patterns look same as negative

### Switching Rules

```powershell
# At runtime: Use trackbar "Rule" (0-3)
0 → Conway's Life
1 → HighLife
2 → Seeds
3 → Day & Night

# Or in code:
CARule custom = CARule::FromString("B36/S23");  // HighLife
```

### Pattern Compatibility

⚠️ **Warning**: Patterns designed for Conway's Life may not work in other rules!

**Example**:
- Glider in HighLife → Behaves differently
- Blinker in Seeds → Dies immediately

**To test**: Load pattern, switch rule, observe behavior.

---

## 📤 Sharing Patterns

### Create Pattern File

```
#N My Amazing Pattern
#O Your Name
#C Description of what it does
#C Discovered on 2024-01-15
x = 10, y = 10, rule = B3/S23
<...RLE data...>!
```

### Share on LifeWiki

1. Verify pattern is novel (search LifeWiki)
2. Create account on ConwayLife forums
3. Post discovery with RLE and animation
4. Community will analyze and add to wiki

### Create Animation

```powershell
# Use advanced version to record MP4
.\gameoflife_advanced.exe -p pattern.rle -v output.mp4 -g 100

# Share video on:
- YouTube
- Reddit r/cellular_automata
- ConwayLife forums
```

---

## 📚 Pattern Resources

### Books
- "The Recursive Universe" by William Poundstone
- "Winning Ways for Your Mathematical Plays" by Berlekamp, Conway, Guy

### Websites
- https://conwaylife.com/wiki/ - LifeWiki
- https://conwaylife.com/forums/ - Forums
- https://playgameoflife.com/ - Interactive
- https://copy.sh/life/ - Pattern editor

### Papers
- "Winning Ways" (1982) - Original glider gun
- "Computation in Cellular Automata" - Theory

---

## 🎯 Pattern Challenges

Test your skills with these challenges:

1. **Find smallest still life with 10 cells**
2. **Create period-17 oscillator**
3. **Design a new spaceship (good luck!)**
4. **Build gun with period < 30**
5. **Create pattern that stabilizes after exactly 1000 gens**

---

## 🛠️ Pattern Utilities

### Batch Process Patterns

```python
import os
import subprocess

pattern_dir = "patterns/"
for file in os.listdir(pattern_dir):
    if file.endswith(".rle"):
        # Run simulation
        cmd = f"gameoflife_advanced.exe -p {file} -g 1000 -l {file}.csv"
        subprocess.run(cmd, shell=True)
        
        # Analyze results
        df = pd.read_csv(f"{file}.csv")
        print(f"{file}: Final population = {df['alive_cells'].iloc[-1]}")
```

### Pattern Statistics

```python
def analyze_pattern(rle_file):
    """Extract pattern properties"""
    df = run_simulation(rle_file, generations=1000)
    
    return {
        'initial_population': df['alive_cells'].iloc[0],
        'final_population': df['alive_cells'].iloc[-1],
        'max_population': df['alive_cells'].max(),
        'is_stable': df['alive_cells'].iloc[-100:].nunique() == 1,
        'period': detect_period(df['alive_cells']),
        'avg_fps': df['fps'].mean(),
    }

# Run on all patterns
for pattern in patterns:
    stats = analyze_pattern(pattern)
    print(f"{pattern}: {stats}")
```

---

## 🎉 Example Pattern Library

Here are 10 must-have patterns to get started:

### 1. Glider
```
x = 3, y = 3
bob$2bo$3o!
```

### 2. Blinker
```
x = 3, y = 1
3o!
```

### 3. Toad
```
x = 4, y = 2
b3o$3o!
```

### 4. Beacon
```
x = 4, y = 4
2o$2o$2b2o$2b2o!
```

### 5. Pulsar
```
x = 13, y = 13
2b3o3b3o$14$o4bobo4bo$o4bobo4bo$o4bobo4bo$2b3o3b3o$14$2b3o3b3o$o4bobo4bo$
o4bobo4bo$o4bobo4bo$14$2b3o3b3o!
```

### 6. Glider Gun (Gosper)
```
x = 36, y = 9
24bo$22bobo$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o$2o8bo3bob2o4b
obo$10bo5bo7bo$11bo3bo$12b2o!
```

### 7. LWSS (Lightweight Spaceship)
```
x = 5, y = 4
bo2bo$o$o3bo$4o!
```

### 8. Pentadecathlon
```
x = 10, y = 3
3o2b3o$o2bo2bo2bo$3o2b3o!
```

### 9. R-pentomino (Methuselah)
```
x = 3, y = 3
b2o$2o$bo!
```

### 10. Block (Still Life)
```
x = 2, y = 2
2o$2o!
```

Save these to `.rle` files and load them!

---

## 🚀 Next Steps

1. Download patterns from LifeWiki
2. Create your own pattern collection
3. Test patterns in different rules (HighLife, Seeds, etc.)
4. Share discoveries with the community
5. Try pattern engineering challenges

**Happy pattern hunting! 🎨**
