"""Seeds a run actually reached survive the mid-instruction guard.

The guard rejects a seed that lands inside an instruction the linear sweep
already decoded, unless the seed decodes as a prologue. It was written for
RTTI vtable slots, which are inferences (HL2 had two bad ones in 12,288). A
seed from tools.seed_from_log is where the CPU actually went, so it is kept
even without a prologue: Halo's XPP init at 0x001CF6AC opens `cmp [flag], 0`
right after a pointer table the sweep had walked as code.
"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.disasm.__main__ import _load_seed_functions  # noqa: E402
from tools.disasm.disasm import Disassembler  # noqa: E402


class _Engine:
    def __init__(self, prologues, padded=()):
        self._prologues = set(prologues)
        self._padded = set(padded)

    def probes_as_prologue(self, addr):
        return addr in self._prologues

    def probes_after_padding(self, addr):
        return addr in self._padded


def _disasm(observed, prologues, padded=()):
    d = Disassembler.__new__(Disassembler)
    d.observed_seeds = set(observed)
    d.engine = _Engine(prologues, padded)
    return d


class ObservedSeedTest(unittest.TestCase):
    def test_observed_seed_is_trusted_without_a_prologue(self):
        d = _disasm(observed={0x001CF6AC}, prologues=())
        self.assertTrue(d._trust_mid_instruction_seed(0x001CF6AC))

    def test_unobserved_non_prologue_seed_is_still_rejected(self):
        # The HL2 RTTI case: six bytes into a mov, decodes as nothing.
        d = _disasm(observed=(), prologues=())
        self.assertFalse(d._trust_mid_instruction_seed(0x00202C2E))

    def test_seed_after_int3_padding_is_trusted(self):
        # Ninja Gaiden Black's frameless leaf at 0x000F7980 (`mov ecx,
        # [esp+4]`), after a byte table and int3 padding the sweep ran through.
        d = _disasm(observed=(), prologues=(), padded={0x000F7980})
        self.assertTrue(d._trust_mid_instruction_seed(0x000F7980))

    def test_prologue_seed_is_trusted(self):
        d = _disasm(observed=(), prologues={0x00069538})
        self.assertTrue(d._trust_mid_instruction_seed(0x00069538))

    def test_verified_static_branch_is_not_a_runtime_observation(self):
        d = _disasm(observed=(), prologues=())
        d.static_branch_seeds = {0x00DCA200}
        self.assertTrue(d._trust_mid_instruction_seed(0x00DCA200))
        self.assertNotIn(0x00DCA200, d.observed_seeds)
        self.assertFalse(d._trust_mid_instruction_seed(0x00202C2E))

    def test_loader_keeps_static_branch_provenance_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, 'seeds.json')
            with open(path, 'w') as stream:
                json.dump([{'start':'0x00DCA200','static_branch':True}], stream)
            observed, branches = set(), set()
            self.assertEqual(_load_seed_functions(path, observed, branches), [0x00DCA200])
            self.assertFalse(observed)
            self.assertEqual(branches, {0x00DCA200})

    def test_loader_marks_observed_seeds(self):
        seeds = [
            {"start": "0x001CF6AC", "observed": True, "note": "x"},
            # Written by seed_from_log before the field existed.
            {"start": "0x00015C5A", "note": "PsCreateSystemThreadEx "
             "StartContext1 observed at runtime; decodes as a function body."},
            {"start": "0x00202C2E", "note": "RTTI vtable slot"},
            0x00010000,
        ]
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "seeds.json")
            with open(path, "w") as f:
                json.dump(seeds, f)
            observed = set()
            addrs = _load_seed_functions(path, observed)
        self.assertEqual(sorted(addrs),
                         [0x00010000, 0x00015C5A, 0x001CF6AC, 0x00202C2E])
        self.assertEqual(observed, {0x001CF6AC, 0x00015C5A})


if __name__ == "__main__":
    unittest.main()
