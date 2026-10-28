"""Synthetic recipe fixtures; no game data or graphics device required."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('seed', Path(__file__).resolve().parents[1] / 'tools/merge_d3d_seed.py')
seed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(seed)


def record(kind, key, payload):
    return seed.HEADER.pack(kind, key, len(payload)) + payload


class SeedMerge(unittest.TestCase):
    def fixture(self):
        program = b'SYNTHETICPROGRAM'  # Four synthetic words, never real instructions.
        vertex = bytes(11) + struct.pack('<I', 4) + program
        pipe = struct.pack('<QQ', 7, 9) + bytes(240)
        data = seed.MAGIC + record(b'V', 7, vertex) + record(b'P', 9, bytes(340)) + record(b'K', 0, pipe)
        return program, data

    def test_offsets_only_and_duplicate_inputs(self):
        program, data = self.fixture()
        out, counts = seed.merge([data, data], b'prefix' + program + b'suffix')
        self.assertNotIn(program, out)
        parsed = list(seed.records(out))
        self.assertEqual([r[0] for r in parsed], [b'W', b'P', b'K'])
        self.assertEqual(struct.unpack_from('<II', parsed[0][2], 11), (6, 4))
        self.assertEqual(counts['K'], 1)

    def test_unknown_program_cannot_leave_a_dangling_pipeline(self):
        _, data = self.fixture()
        out, counts = seed.merge([data], b'not-the-program')
        self.assertEqual([r[0] for r in seed.records(out)], [b'P'])
        self.assertEqual(counts['pipelines_without_recipes'], 1)

    def test_damaged_inputs_fail_instead_of_becoming_a_seed(self):
        _, data = self.fixture()
        for damaged in (data[:-1], data[:10], b'WRONGHDR', seed.MAGIC + record(b'Z', 0, b'')):
            with self.assertRaises(ValueError):
                seed.merge([damaged], b'x' * 100)

    def test_existing_reference_bounds(self):
        data = seed.MAGIC + record(b'W', 7, bytes(11) + struct.pack('<II', 90, 4))
        with self.assertRaises(ValueError):
            seed.merge([data], bytes(100))

    def test_conflicting_versions_are_rejected(self):
        a = seed.MAGIC + record(b'P', 9, bytes(340))
        b = seed.MAGIC + record(b'P', 9, b'x' + bytes(339))
        with self.assertRaises(ValueError):
            seed.merge([a, b], b'')


if __name__ == '__main__':
    unittest.main()
