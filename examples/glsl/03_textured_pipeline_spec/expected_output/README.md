# Expected bundle layout

After a successful run, `output/textured_bundle/` contains SPIR-V, MSL, and reflection for vertex + fragment stages. The fragment stage reflection should include the `uTexture` sampler binding with Metal texture/sampler slots populated.
