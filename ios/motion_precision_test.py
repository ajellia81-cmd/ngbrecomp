"""Check that the production shader wrapper consumes VF3 geometry only on replay."""
from pathlib import Path
import sys

def main():
    folder = Path(sys.argv[1])
    for name in ["motion-fixed.vert.hlsl", "motion-program.vert.hlsl", "motion-decoded.vert.hlsl"]:
        source = (folder / name).read_text()
        assert "[[vk::binding(10, 0)]] StructuredBuffer<float4> scene_positions" in source
        assert "uint scene_vertex : SV_VertexID" in source
        assert "if (motion.x > 0)" in source
        assert "scene_positions[scene_vertex]" in source
        assert "motion_vc" not in source
        assert "motion_previous" not in source
    root = Path(__file__).resolve().parents[1]
    assert not (root / "src/d3d/gpu/motion.h").exists()
    print("Original endpoints stay guarded; replay consumes VF3 scene positions; legacy constant-bank interpolation removed. GPU execution is not implied.")

if __name__ == "__main__":
    main()
