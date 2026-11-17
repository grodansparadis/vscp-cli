#!/usr/bin/env python3
"""Tests for tools/update_version.py (run: python3 -B tests/test_update_version.py)."""

import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import update_version  # noqa: E402

HEADER = ROOT / "src" / "vscp-cli-version.h"
VARIABLES = ROOT / "docs" / "variables.xml"
CMAKELISTS = ROOT / "CMakeLists.txt"


class UpdateVersionTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.header = pathlib.Path(self.dir) / "vscp-cli-version.h"
        self.variables = pathlib.Path(self.dir) / "variables.xml"
        shutil.copy(HEADER, self.header)
        shutil.copy(VARIABLES, self.variables)

    def tearDown(self):
        shutil.rmtree(self.dir)

    def set_version(self, version):
        self.header.write_text(update_version.write_version(self.header.read_text(), version))

    def run_update(self, date):
        update_version.main(["--header", str(self.header), "--variables", str(self.variables), "--date", date])
        return update_version.read_version(self.header.read_text())

    def test_repository_header_is_valid(self):
        update_version.read_version(HEADER.read_text())

    def test_default_header_is_valid(self):
        self.assertEqual(0, update_version.main(["--check"]))

    def test_same_month_increments_patch(self):
        self.set_version((26, 11, 4))
        self.assertEqual((26, 11, 5), self.run_update("2026-11"))
        self.assertEqual((26, 11, 6), self.run_update("2026-11"))

    def test_new_month_resets_patch(self):
        self.set_version((26, 11, 17))
        self.assertEqual((26, 12, 0), self.run_update("2026-12"))

    def test_new_year_resets_patch(self):
        self.set_version((26, 12, 9))
        self.assertEqual((27, 1, 0), self.run_update("2027-01"))

    def test_string_and_number_follow(self):
        self.set_version((26, 11, 2))
        self.run_update("2026-11")
        text = self.header.read_text()
        self.assertIn('VSCPCLI_VERSION_STR   "26.11.3"', text)
        self.assertIn("VSCPCLI_VERSION       0x1A0B0003UL", text)

    def test_document_version_follows_header(self):
        variables = (
            "<var>\n"
            "    <creation-time>unchanged</creation-time>\n"
            "    <document-version>1.2.3</document-version>\n"
            "</var>\n"
        )
        self.variables.write_text(variables)
        self.set_version((26, 11, 4))

        self.assertEqual((26, 11, 5), self.run_update("2026-11"))
        self.assertEqual(variables.replace("1.2.3", "26.11.5"), self.variables.read_text())
        self.assertEqual((26, 12, 0), self.run_update("2026-12"))
        self.assertEqual(variables.replace("1.2.3", "26.12.0"), self.variables.read_text())

    def test_default_paths_update_document_version(self):
        self.set_version((26, 11, 4))
        project = pathlib.Path(self.dir) / "project"
        for directory in ("src", "docs", "tools"):
            (project / directory).mkdir(parents=True)
        header = project / "src" / self.header.name
        variables = project / "docs" / self.variables.name
        script = project / "tools" / "update_version.py"
        shutil.copy(self.header, header)
        shutil.copy(self.variables, variables)
        shutil.copy(ROOT / "tools" / script.name, script)

        result = subprocess.run(
            [sys.executable, "-B", str(script), "--date", "2026-11"],
            cwd=self.dir, capture_output=True, text=True, check=True
        )
        self.assertEqual("26.11.5", result.stdout.strip())
        self.assertEqual((26, 11, 5), update_version.read_version(header.read_text()))
        self.assertEqual("26.11.5", ET.parse(variables).findtext("document-version"))

    def test_custom_header_does_not_update_default_document(self):
        self.set_version((26, 11, 4))
        before = self.variables.read_text()
        with mock.patch.object(update_version, "DEFAULT_VARIABLES", self.variables):
            update_version.main(["--header", str(self.header), "--date", "2026-11"])
        self.assertEqual((26, 11, 5), update_version.read_version(self.header.read_text()))
        self.assertEqual(before, self.variables.read_text())

    def test_invalid_document_variables_do_not_change_header(self):
        self.set_version((26, 11, 4))
        header = self.header.read_text()
        documents = (
            "<var/>",
            "<var><document-version>1</document-version><document-version>2</document-version></var>",
            "<var><document-version>1</document-version>",
        )
        for document in documents:
            with self.subTest(document=document):
                self.variables.write_text(document)
                with self.assertRaises((ValueError, ET.ParseError)):
                    self.run_update("2026-11")
                self.assertEqual(header, self.header.read_text())
                self.assertEqual(document, self.variables.read_text())

    def test_document_write_failure_does_not_change_header(self):
        self.set_version((26, 11, 4))
        header = self.header.read_text()
        write_text = pathlib.Path.write_text

        def fail_document_write(path, *args, **kwargs):
            if path == self.variables:
                raise OSError("cannot write documentation variables")
            return write_text(path, *args, **kwargs)

        with mock.patch.object(pathlib.Path, "write_text", autospec=True, side_effect=fail_document_write):
            with self.assertRaises(OSError):
                self.run_update("2026-11")
        self.assertEqual(header, self.header.read_text())

    def test_date_before_version_is_refused(self):
        self.set_version((26, 11, 0))
        with self.assertRaises(ValueError):
            self.run_update("2026-10")

    def test_check_does_not_change(self):
        before = self.header.read_text()
        variables = self.variables.read_text()
        update_version.main(["--header", str(self.header), "--variables", str(self.variables), "--check"])
        self.assertEqual(before, self.header.read_text())
        self.assertEqual(variables, self.variables.read_text())

    def test_cmake_reads_the_same_fields(self):
        # CMakeLists.txt takes the project version from the header
        cmake = CMAKELISTS.read_text()
        self.assertIn("src/vscp-cli-version.h", cmake)
        self.assertTrue(re.search(r"VSCPCLI_VERSION_\(YEAR\|MONTH\|PATCH\)", cmake))


if __name__ == "__main__":
    unittest.main()
