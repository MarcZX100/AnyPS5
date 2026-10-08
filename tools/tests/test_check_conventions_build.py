import os
import subprocess
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import patch

from tools import check_conventions


class CheckBuildTests(unittest.TestCase):
    def test_system_dependency_detection_handles_find_package_syntax(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}

            def run_git(*args, text=True):
                result = subprocess.run(
                    ["git", *args], cwd=repo, env=env, capture_output=True, check=True
                )
                return result.stdout.decode("utf-8") if text else result.stdout

            def git(*args):
                subprocess.run(
                    ["git", *args], cwd=repo, env=env, capture_output=True, check=True
                )

            git("init", "--quiet")
            git("checkout", "--quiet", "-b", "base")
            git("config", "user.name", "Test")
            git("config", "user.email", "test@example.invalid")
            git("config", "commit.gpgsign", "false")
            cmake = repo / "CMakeLists.txt"
            cmake.write_text(
                "project(anyps5)\nfind_package(\n    Python3 REQUIRED\n)\n"
                "find_package(ExistingSystem REQUIRED)\nfind_package (\n"
                "    OriginalOnly REQUIRED\n)\n",
                encoding="utf-8",
            )
            git("add", "CMakeLists.txt")
            git("commit", "--quiet", "-m", "build: test base")
            git("checkout", "--quiet", "-b", "head")
            cmake.write_text(
                "\n".join(
                    (
                        "project(anyps5)",
                        "find_package(",
                        "    Vulkan REQUIRED",
                        ")",
                        "find_package(ExistingSystem REQUIRED)",
                        "FIND_PACKAGE (",
                        "    OriginalOnly REQUIRED",
                        ")",
                        "find_package(Vulkan REQUIRED)",
                        "find_package( Vulkan REQUIRED)",
                        "find_package(${DEP} REQUIRED)",
                        "FIND_PACKAGE(OpenSSL REQUIRED)",
                        "find_package( Python3 REQUIRED)",
                        "find_package(",
                        "    # package may follow a comment",
                        "    Boost REQUIRED",
                        ")",
                        "find_package(",
                        "    LibArchive REQUIRED",
                        ")",
                        "find_package(",
                        "    Threads REQUIRED",
                        ")",
                        "find_package(Git REQUIRED)",
                        "PKG_CHECK_MODULES(ZLIB REQUIRED zlib)",
                        "Pkg_Search_Module(FOO foo)",
                        "FIND_LIBRARY(M_LIBRARY m)",
                        "# find_package(CommentOnly)",
                        "",
                    )
                ),
                encoding="utf-8",
            )
            cmake_dir = repo / "cmake"
            cmake_dir.mkdir()
            (cmake_dir / "Deps.cmake").write_text(
                "# comment\u2028continued\nfind_package(\n    ExternalThing REQUIRED\n)\n",
                encoding="utf-8",
            )
            git("add", "CMakeLists.txt")
            git("add", "cmake/Deps.cmake")
            git("commit", "--quiet", "-m", "build: add package lookups")

            with patch.object(check_conventions, "git", side_effect=run_git):
                check = check_conventions.Check("base", "HEAD")
                check.build()

            findings = [
                finding
                for finding in check.findings
                if finding[0] == "system-dependency"
            ]
            self.assertEqual(
                Counter(finding[3] for finding in findings),
                Counter(
                    {
                        "find_package(Vulkan)": 3,
                        "find_package(OriginalOnly)": 1,
                        "find_package(${DEP})": 1,
                        "find_package(OpenSSL)": 1,
                        "find_package(Boost)": 1,
                        "find_package(LibArchive)": 1,
                        "PKG_CHECK_MODULES(ZLIB REQUIRED zlib)": 1,
                        "Pkg_Search_Module(FOO foo)": 1,
                        "FIND_LIBRARY(M_LIBRARY m)": 1,
                        "find_package(ExternalThing)": 1,
                    }
                ),
            )
            self.assertNotIn("find_package(ExistingSystem)", [finding[3] for finding in findings])
            self.assertIn(
                ("system-dependency", "CMakeLists.txt", 3, "find_package(Vulkan)"),
                findings,
            )
            self.assertIn(
                ("system-dependency", "CMakeLists.txt", 6, "find_package(OriginalOnly)"),
                findings,
            )
            self.assertIn(
                ("system-dependency", "cmake/Deps.cmake", 3, "find_package(ExternalThing)"),
                findings,
            )


if __name__ == "__main__":
    unittest.main()
