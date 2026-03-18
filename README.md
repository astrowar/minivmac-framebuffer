# minivmac-framebuffer

MiniVmac framebuffer port for Linux - Mac II/Plus emulator with direct framebuffer output.

## Building

```bash
cd src
make
```

## Usage

```bash
./minivmac [options] [rom] [disk]
```

### Options

| Option | Description |
|--------|-------------|
| `-r rom` | Specify ROM file (Mac II or Mac Plus) |
| `-d dir` | Specify disk directory |
| `--skip n` | Skip n frames between draws |
| `--rotate deg` | Rotate output (0, 90, 180, 270) |
| `--scale f` | Scale factor (e.g., 1.15) |
| `--offset-x n` | Horizontal offset in pixels (positive = right) |
| `--offset-y n` | Vertical offset in pixels (positive = down) |
| `--snapshot path` | Take a snapshot to PPM file |
| `--test` | Run in test mode (memory buffer, no framebuffer) |
| `-h` | Show this help |

### Examples

```bash
# Run with default ROM and disk
./minivmac

# Specify ROM and disk image
./minivmac -r rom/MacII.rom -d local/

# Rotate 90 degrees and scale
./minivmac --rotate 90 --scale 1.5

# Offset the display by 50 pixels right and 30 down
./minivmac --offset-x 50 --offset-y 30

# Skip 2 frames between draws (reduce CPU usage)
./minivmac --skip 2

# Test mode (no framebuffer, prints ASCII art to console)
./minivmac --test
```

## Requirements

- Linux with framebuffer support (`/dev/fb0`)
- GCC compiler
- X11 development headers (for compilation)

## License

MIT