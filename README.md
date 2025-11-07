# ic-proj2
Second Lab Work of IC Course

## Build

Build using the Makefile inside `src/` (binary will be at `src/build/extract_color_channel`):

```bash
make -C src
```

## Run

Run the binary directly after building:

### Ex1

```bash
./src/build/extract_color_channel <input_image> <output_image> <channel_index>
```

Channel index (OpenCV BGR): 0=Blue, 1=Green, 2=Red

## Clean

Remove the compiled binary:

```bash
make -C src clean
```
