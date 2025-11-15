# ic-proj2

Second Lab Work of IC Course

This repository contains two independent components:

1. **Golomb Coding** — A C++ implementation of Golomb encoding/decoding for integers.
2. **Extract Color Channel** — An OpenCV-based image utility to extract a single color channel.

Both can be built from the same Makefile using different targets.

---

## 🧱 Build

On the root project directory, you can build everything or each target separately:

### Build both (default)

```bash
make
```

### Build only the OpenCV example

```bash
make extract
```

### Build only the Golomb coder

```bash
make golomb
```

The binaries are placed inside the `build/` directory:

* `build/golomb`
* `build/extract_color_channel`
* `build/image_transform`

---

## ▶️ Run

### Exercise 1 — Extract Color Channel (OpenCV)

```bash
./build/extract_color_channel <input_image> <output_image> <channel_index>
```

**Channel index (BGR):**

* `0` → Blue
* `1` → Green
* `2` → Red

Example:

```bash
./build/extract_color_channel images-ppm/airplane.ppm airplane_red.pgm 2
```

### Exercise 2 — Image Transformations (pixel-by-pixel)

Create negatives, mirrors, rotate by multiples of 90°, and brightness changes.

Usage:

```bash
./build/image_transform <input> <output> <operation> [param]
```

Operations:

- `neg` — negative
- `mirror_h` — horizontal mirror
- `mirror_v` — vertical mirror
- `rotate <k>` — rotate by k*90 degrees (k integer)
- `bright <delta>` — add delta to all channels (delta can be negative)

Examples:

```bash
./build/image_transform images-ppm/lena.ppm build/lena_neg.ppm neg
./build/image_transform images-ppm/lena.ppm build/lena_rot90.ppm rotate 1
./build/image_transform images-ppm/lena.ppm build/lena_bright_plus50.ppm bright 50
```

---

### Exercise 3 — Golomb Coding

You can encode or decode integer sequences using Golomb coding.
Run with:

```bash
./build/golomb -m <m_value> -mode <mode> encode <list_of_integers>
```

**Parameters:**

* `-m <m_value>` → the Golomb parameter (must be ≥ 1)
* `-mode <mode>` → negative number handling mode:

  * `sign` → sign-and-magnitude
  * `interleave` → positive/negative interleaving (zig-zag)

**Example (interleaved mode):**

```bash
./build/golomb -m 3 -mode interleave encode 0 -1 5 10
```

---

## 🧹 Clean

To remove the compiled binaries:

```bash
make clean
```

---

## ⚙️ Notes

* The Makefile automatically checks for OpenCV via `pkg-config`.
  If the OpenCV package isn’t found, you can specify one manually:

  ```bash
  make extract PKG=opencv
  ```

* The Golomb module has **no external dependencies** and can always be built.

---
