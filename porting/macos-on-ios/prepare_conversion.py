#!/usr/bin/env python3
"""Add the UI bridge to the pinned converter; do not patch game shader behavior."""
import argparse,shutil
from pathlib import Path

def prepare(compat):
 root=Path(__file__).resolve().parent;overrides=compat/'shim/overrides'
 shutil.copy2(root/'CoreGraphics_NGB.m',overrides/'CoreGraphics.m')
 for name in ('AppKit_NGB.m','bridge.h'):shutil.copy2(root/name,overrides/name)
 (overrides/'korean').mkdir(exist_ok=True)
 shutil.copy2(root/'korean/haptics.h',overrides/'korean/haptics.h')
 shutil.copy2(root/'korean/haptics.m',overrides/'AppKit_NGBHaptics.m')
 for name in ('haptics.h','haptics_policy.h'):shutil.copy2(root/'korean'/name,overrides/name)
 # This converter-owned periodic debug logger is not part of the Mac game.
 (overrides/'AppKit_Stats.m').write_text('/* No conversion-owned periodic statistics. */\n')
 p=compat/'tools/convert_app.py';s=p.read_text();old='"-framework", "Metal", "-framework", "GameController"]'
 if s.count(old)!=1:raise ValueError('Pinned converter linker arguments changed')
 s=s.replace(old,'"-framework", "Metal", "-framework", "GameController", "-framework", "CoreHaptics", "-framework", "AVFoundation", "-framework", "UniformTypeIdentifiers"]')
 p.write_text(s)
 print('Compatibility UI bridge installed; Mac renderer/compiler unchanged')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('compat',type=Path);prepare(a.parse_args().compat)
