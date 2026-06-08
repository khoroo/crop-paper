# crop-paper

Simple C & Raylib program to crop images for desktop wallpapers.

## Usage

```
crop-paper <W:H> <image> --output <path>
```

Controls:
- `h`/`j`/`k`/`l` — nudge crop area
- `a`/`s` — scale crop area in/out
- `f` — toggle focus view
- `Enter` — save and quit
- `Esc`/`q` — quit without saving
- `?` — toggle help
- Number prefix repeats an action N times

## Dependencies

- **C23 compiler** (e.g. GCC, Clang)
- **Raylib** (>= 4.5)
- **pkg-config**
- **Python 3** with `fonttools` (for font subsetting at build time)
- **Iosevka** font (or replace with your preferred monospace font)

Build requires `raylib` development headers/libraries and `libm`.

## LLM Declaration
produced with DeepSeek Flash V4 & OpenCode
