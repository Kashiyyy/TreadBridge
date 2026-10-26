# Display preview

Renders the **real display code** (`src/ui.cpp`) on a PC and produces the pictures used in the main README.
A tiny software replacement for `Arduino_GFX` (`stubs/`) draws into a frame buffer, so what you see is what
the code draws on the panel (same 5x7 font, same rectangle/circle algorithms).

```bash
pip install pillow
./tools/display-preview/build.sh      # writes docs/images/display-portrait.png and display-landscape.png
```

Run it again after changing `ui.cpp` to refresh the pictures. Demo values (speed, devices, names) are set in `fill()` in `render.cpp`.
