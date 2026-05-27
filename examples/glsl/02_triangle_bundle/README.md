# 02_triangle_bundle

File-based GLSL shaders (position-only triangle) compiled to an MSL bundle. Prints reflection bindings and writes `./output/triangle_bundle/`.

```bash
cmake --build build --target vnesc_example_02_triangle_bundle
cd build/bin/examples/glsl/02_triangle_bundle
./vnesc_example_02_triangle_bundle
```

See `expected_output/README.md` for the expected bundle layout.
