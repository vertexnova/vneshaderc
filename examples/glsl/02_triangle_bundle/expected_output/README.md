# Expected bundle layout

After a successful run, `output/triangle_bundle/` contains:

| File | Description |
|------|-------------|
| `bundle.header` | Binary stage index |
| `reflection.bin` | Program reflection (may be empty bindings for this shader) |
| `vertex.spv` / `fragment.spv` | SPIR-V per stage |
| `vertex.msl` / `fragment.msl` | Cross-compiled MSL |

With `VNE_SC_JSON=ON`, `manifest.json` is also written.
