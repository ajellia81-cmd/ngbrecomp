import unittest
from report_build_errors import summaries


class PrivateDiagnosticTests(unittest.TestCase):
    def test_lifted_source_and_context_are_not_published(self):
        text = """/repo/src/recomp/gen/chunk.c:17:4: error: private_game_expression
  private_game_expression();
  ^
/repo/src/install.c:473:11: error: 'fork' is unavailable: not available on iOS
  pid = fork();
https://private.example/download?secret=value error: bad response
"""
        output = "\n".join(summaries(text))
        self.assertNotIn("private_game_expression", output)
        self.assertNotIn("pid =", output)
        self.assertNotIn("private.example", output)
        self.assertIn("install.c:473:11", output)
        self.assertIn("not available on iOS", output)


if __name__ == "__main__":
    unittest.main()
