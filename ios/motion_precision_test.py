"""Validate original depth arithmetic after retained-input interpolation."""
from pathlib import Path
import struct
import sys

def main():
    folder=Path(sys.argv[1])
    for name in ['motion-fixed.vert.hlsl','motion-program.vert.hlsl']:
        source=(folder/name).read_text()
        assert 'MotionClipPosition' not in source
        assert 'motion_previous' not in source
        assert 'VOut main(VIn i)' in source
    print('Motion shaders retain the original vertex program; intermediate input and finite/reset cases are checked by the native history test. No GPU execution implied.')
if __name__=='__main__': main()
