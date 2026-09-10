#!/usr/bin/env python3
"""Stage and zip Ritual Codec build outputs by plug-in format.

The script intentionally uses only the Python standard library. It handles
single- and multi-config CMake output layouts and leaves the build tree alone.
"""

from __future__ import annotations

import argparse
import os
import stat
import subprocess
import shutil
import sys
import zipfile
import tempfile
import hashlib
from datetime import datetime, timezone
from pathlib import Path


JUCE_COMMIT = "72782788ce18c2d4d760b28e0921d6ffc6431102"
JUCE_VERSION = "9.0.2"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--platform", required=True,
                        help="Artifact platform label, for example linux-x64")
    parser.add_argument("--kind", action="append", choices=["cli", "vst3", "au"],
                        help="Package only this kind (repeatable); defaults to enabled CMake targets")
    return parser.parse_args()


def unique_bundles(build_dir: Path, suffix: str) -> list[Path]:
    """Return top-level bundles, omitting nested matches and duplicates."""
    candidates = sorted(
        (path for path in build_dir.rglob(f"*{suffix}") if path.is_dir()),
        key=lambda path: (len(path.parts), str(path)),
    )
    selected: list[Path] = []
    for candidate in candidates:
        if any(candidate == previous or previous in candidate.parents
               for previous in selected):
            continue
        selected.append(candidate)
    return selected


def find_cli(build_dir: Path) -> Path:
    names = {"ritual-codec", "ritual-codec.exe"}
    candidates = sorted(path for path in build_dir.rglob("*")
                        if path.is_file() and path.name in names)
    if not candidates:
        raise RuntimeError(
            f"could not find ritual-codec executable below {build_dir}")
    # The exact target name is unique in a normal CMake tree. Prefer a path that
    # is not in a CMake helper/dependency directory if an IDE generated extras.
    candidates.sort(key=lambda path: ("_deps" in path.parts, len(path.parts), str(path)))
    return candidates[0]


def locate_license(root: Path) -> Path:
    for name in ("LICENSE", "LICENSE.md", "COPYING"):
        path = root / name
        if path.is_file():
            return path
    raise RuntimeError("a root LICENSE, LICENSE.md, or COPYING file is required")


def copy_common_docs(stage: Path, root: Path, build_dir: Path,
                     artifact_kind: str, platform: str) -> None:
    docs = stage / "docs"
    docs.mkdir(parents=True, exist_ok=True)
    shutil.copy2(locate_license(root), docs / "LICENSE")

    required = [
        (root / "ATTRIBUTION.md", docs / "ATTRIBUTION.md"),
        (root / "LICENSE-NOTES.md", docs / "LICENSE-NOTES.md"),
        (root / "reports" / "source_provenance.json",
         docs / "reports" / "source_provenance.json"),
        (root / "docs" / "BUILDING.md", docs / "BUILDING.md"),
    ]
    missing = [str(source) for source, _ in required if not source.is_file()]
    if missing:
        raise RuntimeError("required provenance/building files are missing: "
                           + ", ".join(missing))
    for source, destination in required:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    shutil.copy2(root / "assets/fonts/OFL.txt", docs / "IBM-Plex-Mono-OFL.txt")
    shutil.copy2(root / "assets/fonts/CormorantGaramond-OFL.txt", docs / "CormorantGaramond-OFL.txt")

    readme = root / "README.md"
    if readme.is_file():
        shutil.copy2(readme, docs / "README.md")

    build_info = build_dir / "RitualCodecBuildInfo.txt"
    resolved_source = "FetchContent pinned commit " + JUCE_COMMIT
    resolved_version = JUCE_VERSION
    if build_info.is_file():
        for line in build_info.read_text(encoding="utf-8").splitlines():
            if line.startswith("JUCE "):
                resolved_version = line.removeprefix("JUCE ").strip()
            elif line.startswith("FetchContent ") or line.startswith("local source"):
                resolved_source = line.strip()

    source_dir = stage / "source"
    source_dir.mkdir(exist_ok=True)
    # Include matching project source directly, even before the GitHub upload succeeds.
    with zipfile.ZipFile(source_dir / "ritual-codec-source.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(root.rglob("*")):
            rel = path.relative_to(root)
            if stage.parent == path or stage.parent in path.parents:
                continue
            if any(part.startswith(("build", "dist")) or part in {".git", "dependencies", "__pycache__"} for part in rel.parts[:-1]):
                continue
            if path.is_file() and path.suffix not in {".wav", ".mp3", ".log", ".pyc", ".zip"}:
                archive.write(path, Path("ritual-codec") / rel)
    (source_dir / "JUCE-SOURCE.txt").write_text(
        "JUCE 9.0.2, unmodified upstream dependency (plugins only):\n"
        f"https://github.com/juce-framework/JUCE/tree/{JUCE_COMMIT}\n"
        f"git clone https://github.com/juce-framework/JUCE.git\n"
        f"git -C JUCE checkout {JUCE_COMMIT}\n"
        "Build the included project source with RITUAL_JUCE_SOURCE_DIR pointing to JUCE.\n",
        encoding="utf-8")
    cache = build_dir / "CMakeCache.txt"
    if artifact_kind != "cli" and cache.exists():
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith(("RITUAL_JUCE_SOURCE_DIR:PATH=", "juce_SOURCE_DIR:STATIC=", "JUCE_SOURCE_DIR:STATIC=")):
                directory = Path(line.split("=", 1)[1])
                for name in ("LICENSE.md", "JUCE.spdx.json"):
                    upstream = directory / name
                    if upstream.is_file():
                        shutil.copy2(upstream, docs / ("JUCE-" + name))
    if artifact_kind != "cli" and not (docs / "JUCE-LICENSE.md").is_file():
        raise RuntimeError("JUCE license was not found in the configured source directory")
    if artifact_kind == "cli":
        resolved_source = "not linked; CLI has no JUCE dependency"
        resolved_version = "not applicable"
    manifest = stage / "BUILD_PROVENANCE.txt"
    manifest.write_text(
        "Ritual Codec build artifact\n"
        f"Artifact kind: {artifact_kind}\n"
        f"Platform label: {platform}\n"
        f"JUCE version: {resolved_version}\n"
        f"JUCE source: {resolved_source}\n"
        f"Packaged (UTC): {datetime.now(timezone.utc).isoformat()}\n",
        encoding="utf-8",
    )


def zip_stage(stage: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    if sys.platform == "darwin":
        subprocess.run(["/usr/bin/ditto", "-c", "-k", "--sequesterRsrc",
                        str(stage), str(archive)], check=True)
        return
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as output:
        for path in sorted(stage.rglob("*")):
            if path.is_symlink():
                info = zipfile.ZipInfo(str(path.relative_to(stage)))
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                output.writestr(info, os.readlink(path))
            elif path.is_file():
                output.write(path, path.relative_to(stage))


def verify_archive(stage: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise RuntimeError(f"CRC failure in {archive}")
    if sys.platform != "darwin":
        return
    # Exercise the native extractor and verify the actual deliverable, not just
    # the pre-packaging bundle. A signature or execute-bit loss must fail CI.
    with tempfile.TemporaryDirectory(prefix="ritual-extract-") as temporary:
        extracted = Path(temporary)
        subprocess.run(["/usr/bin/ditto", "-x", "-k", str(archive), str(extracted)], check=True)
        for original in stage.rglob("*"):
            restored = extracted / original.relative_to(stage)
            if original.is_symlink():
                if not restored.is_symlink() or os.readlink(original) != os.readlink(restored):
                    raise RuntimeError(f"Symlink changed: {original}")
            elif original.is_file():
                if hashlib.sha256(original.read_bytes()).digest() != hashlib.sha256(restored.read_bytes()).digest():
                    raise RuntimeError(f"Extracted bytes differ: {original}")
                if stat.S_IMODE(original.stat().st_mode) != stat.S_IMODE(restored.stat().st_mode):
                    raise RuntimeError(f"Extracted permissions differ: {original}")
        for bundle in list(extracted.rglob("*.component")) + list(extracted.rglob("*.vst3")):
            subprocess.run(["/usr/bin/codesign", "--verify", "--strict", str(bundle)], check=True)


def make_archive(root: Path, build_dir: Path, output_dir: Path,
                 platform: str, kind: str, sources: list[Path]) -> Path:
    if not sources:
        raise RuntimeError(f"no {kind} build output was found")
    stage = output_dir / f".staging-{kind}"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    if kind == "cli":
        destination = stage / "bin" / sources[0].name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(sources[0], destination)
    else:
        destination_root = stage / kind.upper()
        destination_root.mkdir(parents=True, exist_ok=True)
        for source in sources:
            shutil.copytree(source, destination_root / source.name,
                            symlinks=True)

    copy_common_docs(stage, root, build_dir, kind, platform)
    archive = output_dir / f"ritual-codec-{platform}-{kind}.zip"
    if archive.exists():
        archive.unlink()
    zip_stage(stage, archive)
    verify_archive(stage, archive)
    shutil.rmtree(stage)
    return archive


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    root = Path(__file__).resolve().parents[1]
    output_dir = args.output_dir.resolve()
    if not build_dir.is_dir():
        raise RuntimeError(f"build directory does not exist: {build_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    available = {"vst3": unique_bundles(build_dir, ".vst3"),
                 "au": unique_bundles(build_dir, ".component")}
    try:
        available["cli"] = [find_cli(build_dir)]
    except RuntimeError:
        available["cli"] = []
    requested = args.kind
    if requested is None:
        cache = build_dir / "CMakeCache.txt"
        if cache.exists():
            settings = cache.read_text(encoding="utf-8").splitlines()
            requested = [kind for kind in ["cli", "vst3", "au"]
                         if f"RITUAL_BUILD_{kind.upper()}:BOOL=ON" in settings]
        else:
            requested = [kind for kind in ["cli", "vst3", "au"] if available[kind]]
    if not requested:
        raise RuntimeError("no enabled build outputs to package")
    archives = [make_archive(root, build_dir, output_dir, args.platform, kind,
                             available[kind]) for kind in dict.fromkeys(requested)]
    for archive in archives:
        print(archive)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"package_release.py: {error}", file=sys.stderr)
        raise SystemExit(2)
