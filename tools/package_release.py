#!/usr/bin/env python3
"""Stage and zip Ritual Codec build outputs by plug-in format.

The helper deliberately uses only the Python standard library.  It packages
only build outputs that can be identified unambiguously, keeps the project
source list explicit, and records the JUCE source directory used by CMake.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


JUCE_COMMIT = "72782788ce18c2d4d760b28e0921d6ffc6431102"
JUCE_VERSION = "9.0.2"

# These are the only project paths that enter the corresponding-source
# archive.  In particular, do not replace this list with root.rglob(): build
# products, reports, recordings, and local caches are not source files.
SOURCE_FILES = (
    ".gitignore",
    "ATTRIBUTION.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "CMakeLists.txt",
    "LICENSE",
    "LICENSE-NOTES.md",
    "Makefile",
    "README.md",
    "requirements-dev.txt",
)
SOURCE_DIRS = (
    "src",
    "plugin",
    "tests",
    "tools",
)
SOURCE_ASSET_FILES = (
    "assets/fonts/CormorantGaramond-OFL.txt",
    "assets/fonts/CormorantGaramond.ttf",
    "assets/fonts/IBMPlexMono-Regular.ttf",
    "assets/fonts/OFL.txt",
    "assets/fonts/README.md",
)
SOURCE_DOC_FILES = (
    "docs/BITSTREAM.md",
    "docs/BUILDING.md",
    "docs/PACKAGING.md",
    "docs/RELEASE_VALIDATION.md",
)

# CMake's generated binary tree can contain several configurations at once.
# A release package must never silently choose one of them.
CONFIGURATION_NAMES = {
    "debug",
    "release",
    "relwithdebinfo",
    "minsizerel",
    "asan",
    "ubsan",
}
IGNORED_BINARY_PARTS = {".git", "_deps", "cmakefiles", "testing"}


class PackagingError(RuntimeError):
    """Raised when a release package cannot be made unambiguously."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--platform", required=True,
                        help="Artifact platform label, for example linux-x64")
    parser.add_argument("--kind", action="append", choices=["cli", "vst3", "au"],
                        help="Package only this kind (repeatable); defaults to enabled CMake targets")
    return parser.parse_args()


def _binary_search_paths(build_dir: Path, names: set[str]) -> list[Path]:
    """Find output files while ignoring CMake helper/dependency directories."""
    candidates: list[Path] = []
    for path in build_dir.rglob("*"):
        if not path.is_file() or path.name not in names:
            continue
        relative_parts = {part.lower() for part in path.relative_to(build_dir).parts[:-1]}
        if relative_parts.intersection(IGNORED_BINARY_PARTS):
            continue
        candidates.append(path)
    return sorted(candidates, key=lambda path: (len(path.relative_to(build_dir).parts), str(path)))


def _configuration_key(path: Path, build_dir: Path) -> str:
    """Return the explicit CMake configuration component, if any."""
    relative = path.relative_to(build_dir)
    configurations = [part.lower() for part in relative.parts[:-1]
                      if part.lower() in CONFIGURATION_NAMES]
    if len(set(configurations)) > 1:
        return "/".join(configurations)
    return configurations[0] if configurations else "<single-config>"


def _reject_ambiguous_configurations(candidates: list[Path], build_dir: Path,
                                     description: str) -> None:
    if not candidates:
        return
    keys = {_configuration_key(path, build_dir) for path in candidates}
    if len(keys) > 1:
        rendered = ", ".join(str(path) for path in candidates)
        raise PackagingError(
            f"ambiguous {description}: outputs span multiple binary configurations "
            f"({', '.join(sorted(keys))}): {rendered}")


def find_cli_outputs(build_dir: Path) -> list[Path]:
    """Return exactly one output for each required CLI executable.

    Both command-line targets are part of one CLI artifact.  A caller must
    still handle a missing output as a package error so a partial archive can
    never be emitted.
    """
    outputs: list[Path] = []
    missing: list[str] = []
    for target in ("ritual-codec", "ritual-stream"):
        candidates = _binary_search_paths(build_dir, {target, target + ".exe"})
        if not candidates:
            missing.append(target)
            continue
        if len(candidates) > 1:
            rendered = ", ".join(str(path) for path in candidates)
            raise PackagingError(
                f"ambiguous CLI output for {target}: multiple binary configurations "
                f"or duplicate outputs ({rendered})")
        outputs.append(candidates[0])
    if missing:
        raise PackagingError(
            "CLI package requires both ritual-codec and ritual-stream; "
            "missing: " + ", ".join(missing))
    _reject_ambiguous_configurations(outputs, build_dir, "CLI outputs")
    return outputs


def find_cli(build_dir: Path) -> Path:
    """Compatibility helper returning the primary CLI executable.

    Packaging uses :func:`find_cli_outputs`; callers of the older helper still
    receive ``ritual-codec`` after both outputs have been validated.
    """
    return find_cli_outputs(build_dir)[0]


def unique_bundles(build_dir: Path, suffix: str) -> list[Path]:
    """Return top-level bundles, omitting nested matches and duplicates."""
    candidates = sorted(
        (path for path in build_dir.rglob(f"*{suffix}") if path.is_dir()),
        key=lambda path: (len(path.relative_to(build_dir).parts), str(path)),
    )
    selected: list[Path] = []
    for candidate in candidates:
        if any(candidate == previous or previous in candidate.parents
               for previous in selected):
            continue
        selected.append(candidate)
    return selected


def locate_license(root: Path) -> Path:
    for name in ("LICENSE", "LICENSE.md", "COPYING"):
        path = root / name
        if path.is_file():
            return path
    raise PackagingError("a root LICENSE, LICENSE.md, or COPYING file is required")


def _read_cache_values(cache: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not cache.is_file():
        return values
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("//") or ":" not in line or "=" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        key = key_and_type.split(":", 1)[0]
        values[key] = value.strip()
    return values


def _read_build_info(build_dir: Path) -> tuple[str | None, str | None, str | None]:
    """Return (version, source description, local source path) from build info."""
    info = build_dir / "RitualCodecBuildInfo.txt"
    if not info.is_file():
        return None, None, None
    version: str | None = None
    description: str | None = None
    local_path: str | None = None
    for line in info.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if stripped.startswith("JUCE "):
            version = stripped.removeprefix("JUCE ").strip()
        elif stripped.lower().startswith("local source override:"):
            description = stripped
            local_path = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("FetchContent "):
            description = stripped
    return version, description, local_path


def _path_from_cache(value: str, root: Path) -> Path | None:
    if not value:
        return None
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = (root / path).resolve()
    return path.resolve()


def _juce_version_from_header(source: Path) -> str | None:
    header = source / "modules/juce_core/system/juce_StandardHeader.h"
    if not header.is_file():
        return None
    values: dict[str, str] = {}
    for line in header.read_text(encoding="utf-8", errors="replace").splitlines():
        if "#define" not in line:
            continue
        fields = line.split()
        if len(fields) >= 3 and fields[1] in {
            "JUCE_MAJOR_VERSION", "JUCE_MINOR_VERSION", "JUCE_BUILDNUMBER"
        }:
            values[fields[1]] = fields[2]
    if len(values) != 3:
        return None
    return ".".join(values[name] for name in (
        "JUCE_MAJOR_VERSION", "JUCE_MINOR_VERSION", "JUCE_BUILDNUMBER"))


def _resolve_juce_source(root: Path, build_dir: Path) -> tuple[Path, str, str]:
    """Resolve the actual JUCE tree configured by CMake.

    Returns (source directory, provenance description, resolved version).  A
    configured local path always wins over the pinned default description, and
    disagreement between build-info and cache metadata is rejected.
    """
    cache_values = _read_cache_values(build_dir / "CMakeCache.txt")
    info_version, info_description, info_local = _read_build_info(build_dir)

    configured_values: list[tuple[str, Path]] = []
    for key in ("RITUAL_JUCE_SOURCE_DIR", "JUCE_SOURCE_DIR", "juce_SOURCE_DIR"):
        value = cache_values.get(key, "")
        source = _path_from_cache(value, root)
        if source is not None:
            configured_values.append((key, source))
    unique_configured = {source for _, source in configured_values}
    if len(unique_configured) > 1:
        rendered = ", ".join(f"{key}={source}" for key, source in configured_values)
        raise PackagingError("ambiguous JUCE source paths in CMakeCache.txt: " + rendered)

    info_path = _path_from_cache(info_local or "", root)
    if info_path is not None and unique_configured and info_path not in unique_configured:
        configured = next(iter(unique_configured))
        raise PackagingError(
            "JUCE source mismatch between RitualCodecBuildInfo.txt and CMakeCache.txt: "
            f"{info_path} versus {configured}")

    source = info_path or next(iter(unique_configured), None)
    if source is None:
        # A build without a source directory is not sufficient evidence for a
        # corresponding-source bundle.  Do not silently turn the pinned commit
        # into a provenance claim or fetch anything while packaging.
        raise PackagingError(
            "cannot identify the configured JUCE source directory; "
            "CMakeCache.txt or RitualCodecBuildInfo.txt is required for plugin packaging")
    if not source.is_dir():
        raise PackagingError(f"configured JUCE source directory does not exist: {source}")

    cache_version = cache_values.get("RITUAL_JUCE_VERSION") or None
    version = info_version or cache_version or _juce_version_from_header(source) or JUCE_VERSION
    header_version = _juce_version_from_header(source)
    if header_version and version and header_version != version:
        raise PackagingError(
            f"configured JUCE source version mismatch: metadata says {version}, "
            f"source header says {header_version} at {source}")

    if info_path is not None:
        description = "local source override; actual configured source bundled"
    elif cache_values.get("RITUAL_JUCE_SOURCE_DIR", ""):
        description = "local source override; actual configured source bundled"
    else:
        commit = JUCE_COMMIT
        if info_description and info_description.startswith("FetchContent "):
            description = f"{info_description}; configured source bundled"
        else:
            description = f"configured source bundled; project default pin {commit} (not independently verified)"
    return source, description, version


def _iter_project_files(root: Path) -> list[tuple[Path, Path]]:
    """Return (source, archive-relative path) entries from the explicit list."""
    entries: list[tuple[Path, Path]] = []

    def add(relative: str) -> None:
        source = root / relative
        if source.is_file() and not source.is_symlink():
            entries.append((source, Path("ritual-codec") / relative))

    for relative in SOURCE_FILES + SOURCE_ASSET_FILES + SOURCE_DOC_FILES:
        add(relative)
    for directory in SOURCE_DIRS:
        base = root / directory
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*")):
            if not source.is_file() or source.is_symlink():
                continue
            relative = source.relative_to(root)
            if any(part in {".git", "__pycache__"} for part in relative.parts):
                continue
            if source.suffix.lower() not in {".cpp", ".hpp", ".h", ".c", ".mm", ".m", ".py", ".cmake", ".md", ".txt", ".json", ".yml", ".yaml"}:
                continue
            entries.append((source, Path("ritual-codec") / relative))
    # Explicit roots can overlap only if the allowlist is edited carelessly.
    unique: dict[Path, Path] = {}
    for source, relative in entries:
        unique[relative] = source
    return sorted(((source, relative) for relative, source in unique.items()),
                  key=lambda item: item[1].as_posix())


def _zip_write_path(archive: zipfile.ZipFile, source: Path, relative: Path) -> None:
    """Write a regular file or symlink while retaining its type and mode."""
    archive_name = relative.as_posix()
    if source.is_symlink():
        info = zipfile.ZipInfo(archive_name)
        info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        archive.writestr(info, os.readlink(source))
    elif source.is_file():
        archive.write(source, archive_name)


def _write_source_archive(root: Path, destination: Path,
                          juce_source: Path | None = None) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as archive:
        for source, relative in _iter_project_files(root):
            _zip_write_path(archive, source, relative)
        if juce_source is not None:
            # Copy the configured tree into the same source bundle.  This is
            # the exact tree used by CMake, rather than a URL or a re-resolved
            # pinned revision.
            for source in sorted(juce_source.rglob("*")):
                relative = source.relative_to(juce_source)
                if any(part == ".git" or part.lower().startswith("cmake-build")
                       for part in relative.parts):
                    continue
                if any(part.lower() == "build" and not (
                        len(relative.parts) >= 2 and
                        relative.parts[0].lower() == "extras" and
                        relative.parts[1] == "Build")
                       for part in relative.parts[:-1]):
                    continue
                if source.is_file() or source.is_symlink():
                    _zip_write_path(archive, source,
                                    Path("ritual-codec") / "JUCE" / relative)


def _juce_legal_files(source: Path) -> list[Path]:
    """Return all JUCE license, notice, and SPDX inventory files."""
    result: list[Path] = []
    for path in sorted(source.rglob("*")):
        if not (path.is_file() and not path.is_symlink()):
            continue
        relative = path.relative_to(source)
        if any(part == ".git" or part.lower().startswith("cmake-build")
               for part in relative.parts):
            continue
        if any(part.lower() == "build" and not (
                len(relative.parts) >= 2 and
                relative.parts[0].lower() == "extras" and
                relative.parts[1] == "Build")
               for part in relative.parts[:-1]):
            continue
        name = path.name.lower()
        if "license" in name or "notice" in name or "spdx" in name:
            result.append(path)
    return result


def _copy_juce_doc_files(stage_docs: Path, source: Path) -> None:
    for required in ("LICENSE.md", "JUCE.spdx.json", "CMakeLists.txt"):
        if not (source / required).is_file():
            raise PackagingError(f"configured JUCE tree is incomplete: missing {required}")
    legal_files = _juce_legal_files(source)
    if not legal_files:
        raise PackagingError(
            f"configured JUCE source has no license, NOTICE, or SPDX files: {source}")
    # Keep top-level inventory files easy to find alongside the full source
    # bundle.  Nested files remain in their original paths inside the source
    # archive, including every license/NOTICE/SPDX file.
    top_level = [path for path in legal_files
                 if len(path.relative_to(source).parts) == 1]
    for path in top_level:
        destination = stage_docs / ("JUCE-" + path.name)
        shutil.copy2(path, destination)


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
        raise PackagingError("required provenance/building files are missing: "
                             + ", ".join(missing))
    for source, destination in required:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    for relative, destination_name in (
        ("assets/fonts/OFL.txt", "IBM-Plex-Mono-OFL.txt"),
        ("assets/fonts/CormorantGaramond-OFL.txt", "CormorantGaramond-OFL.txt"),
    ):
        source = root / relative
        if not source.is_file():
            raise PackagingError(f"required font attribution file is missing: {source}")
        shutil.copy2(source, docs / destination_name)

    readme = root / "README.md"
    if readme.is_file():
        shutil.copy2(readme, docs / "README.md")

    juce_source: Path | None = None
    resolved_source = "not linked; CLI has no JUCE dependency"
    resolved_version = "not applicable"
    if artifact_kind != "cli":
        juce_source, resolved_source, resolved_version = _resolve_juce_source(root, build_dir)
        _copy_juce_doc_files(docs, juce_source)

    source_dir = stage / "source"
    source_dir.mkdir(exist_ok=True)
    _write_source_archive(root, source_dir / "ritual-codec-source.zip", juce_source)
    if artifact_kind == "cli":
        juce_note = (
            "JUCE is not used by this CLI build; no JUCE dependency is bundled.\n"
        )
    else:
        juce_note = (
            "The configured JUCE source is included in "
            "ritual-codec/JUCE/ inside ritual-codec-source.zip.\n"
            "Build with -DRITUAL_JUCE_SOURCE_DIR=./JUCE after extracting the source.\n"
            f"Provenance: {resolved_source}\n"
        )
    (source_dir / "JUCE-SOURCE.txt").write_text(
        f"JUCE version: {resolved_version}\n" + juce_note,
        encoding="utf-8")

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


def _stage_entries(stage: Path) -> list[Path]:
    return sorted((path for path in stage.rglob("*")
                   if path.is_file() or path.is_symlink()),
                  key=lambda path: path.relative_to(stage).as_posix())


def zip_stage(stage: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    if sys.platform == "darwin":
        subprocess.run(["/usr/bin/ditto", "-c", "-k", "--sequesterRsrc",
                        str(stage), str(archive)], check=True)
        return
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as output:
        for path in _stage_entries(stage):
            relative = path.relative_to(stage)
            if path.is_symlink():
                info = zipfile.ZipInfo(relative.as_posix())
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                output.writestr(info, os.readlink(path))
            elif path.is_file():
                output.write(path, relative)


def _check_archive_path(name: str) -> None:
    path = PurePosixPath(name)
    if not path.parts or path.is_absolute() or ".." in path.parts or not name or "\\" in name or ":" in path.parts[0]:
        raise PackagingError(f"unsafe path in archive: {name!r}")


def verify_archive(stage: Path, archive: Path) -> None:
    """Verify CRCs, exact paths, bytes, symlinks, and mode bits on every host."""
    expected_paths = _stage_entries(stage)
    expected_names = {path.relative_to(stage).as_posix() for path in expected_paths}
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise PackagingError(f"CRC failure in {archive}")
        infos = zipped.infolist()
        all_names = [info.filename for info in infos]
        for name in all_names:
            _check_archive_path(name)
        if len(all_names) != len(set(all_names)):
            raise PackagingError(f"duplicate paths in archive: {archive}")
        # ditto emits directory entries and AppleDouble resource metadata.
        names = [info.filename for info in infos if not info.is_dir()
                 and not (sys.platform == "darwin" and info.filename.startswith("__MACOSX/"))]
        if set(names) != expected_names:
            missing = sorted(expected_names - set(names))
            extra = sorted(set(names) - expected_names)
            detail: list[str] = []
            if missing:
                detail.append("missing " + ", ".join(missing))
            if extra:
                detail.append("unexpected " + ", ".join(extra))
            raise PackagingError(f"archive paths differ from staging tree: {'; '.join(detail)}")

        by_name = {info.filename: info for info in infos}
        for original in expected_paths:
            name = original.relative_to(stage).as_posix()
            info = by_name[name]
            recorded_mode = (info.external_attr >> 16) & 0xFFFF
            original_mode = original.lstat().st_mode
            if sys.platform != "win32" and stat.S_IFMT(recorded_mode) != stat.S_IFMT(original_mode):
                raise PackagingError(f"file type metadata differs for {name}")
            if sys.platform != "win32" and stat.S_IMODE(recorded_mode) != stat.S_IMODE(original_mode):
                raise PackagingError(f"permissions differ for {name}")
            data = zipped.read(info)
            if original.is_symlink():
                if data.decode("utf-8") != os.readlink(original):
                    raise PackagingError(f"symlink target differs for {name}")
            else:
                expected_hash = hashlib.sha256(original.read_bytes()).digest()
                actual_hash = hashlib.sha256(data).digest()
                if expected_hash != actual_hash:
                    raise PackagingError(f"file bytes differ for {name}")

    # Extract regular files into a fresh directory as a second path/hash
    # check.  Python's extractor does not recreate symlinks, so symlinks are
    # validated from their ZIP metadata above.
    with tempfile.TemporaryDirectory(prefix="ritual-extract-") as temporary:
        extracted = Path(temporary)
        if sys.platform == "darwin":
            subprocess.run(["/usr/bin/ditto", "-x", "-k", str(archive), str(extracted)], check=True)
        else:
            with zipfile.ZipFile(archive) as zipped:
                zipped.extractall(extracted)
        for original in expected_paths:
            restored = extracted / original.relative_to(stage)
            if original.is_symlink():
                if sys.platform == "darwin" and (not restored.is_symlink()
                        or os.readlink(restored) != os.readlink(original)):
                    raise PackagingError(f"extracted symlink differs for {original}")
                continue
            if sys.platform == "darwin" and stat.S_IMODE(original.stat().st_mode) != stat.S_IMODE(restored.stat().st_mode):
                raise PackagingError(f"extracted permissions differ for {original}")
            if not restored.is_file():
                raise PackagingError(f"archive extraction omitted {original}")
            if hashlib.sha256(original.read_bytes()).digest() != hashlib.sha256(restored.read_bytes()).digest():
                raise PackagingError(f"extracted bytes differ for {original}")

        if sys.platform == "darwin":
            for bundle in list(extracted.rglob("*.component")) + list(extracted.rglob("*.vst3")):
                subprocess.run(["/usr/bin/codesign", "--verify", "--strict", str(bundle)], check=True)


def _validate_sources(kind: str, sources: list[Path], build_dir: Path) -> None:
    if kind == "cli":
        if len(sources) != 2:
            missing = [name for name in ("ritual-codec", "ritual-stream")
                       if not any(path.name.removesuffix(".exe") == name for path in sources)]
            detail = ", ".join(missing) if missing else "duplicate or ambiguous outputs"
            raise PackagingError(
                "CLI package requires both ritual-codec and ritual-stream; " + detail)
        names = {path.name.removesuffix(".exe") for path in sources}
        if names != {"ritual-codec", "ritual-stream"}:
            raise PackagingError("CLI package contains unexpected executable names")
        _reject_ambiguous_configurations(sources, build_dir, "CLI outputs")
    else:
        if not sources:
            raise PackagingError(f"no {kind} build output was found")
        if len(sources) > 1:
            rendered = ", ".join(str(path) for path in sources)
            raise PackagingError(
                f"ambiguous {kind} output: multiple binary configurations or bundles "
                f"({rendered})")
        _reject_ambiguous_configurations(sources, build_dir, f"{kind} output")


def make_archive(root: Path, build_dir: Path, output_dir: Path,
                 platform: str, kind: str, sources: list[Path]) -> Path:
    _validate_sources(kind, sources, build_dir)
    stage = output_dir / f".staging-{kind}"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    if kind == "cli":
        destination_root = stage / "bin"
        destination_root.mkdir(parents=True, exist_ok=True)
        for source in sources:
            shutil.copy2(source, destination_root / source.name)
    else:
        destination_root = stage / kind.upper()
        destination_root.mkdir(parents=True, exist_ok=True)
        for source in sources:
            shutil.copytree(source, destination_root / source.name,
                            symlinks=True)

    copy_common_docs(stage, root, build_dir, kind, platform)
    sums = []
    for entry in _stage_entries(stage):
        if entry.is_symlink():
            if not entry.resolve().is_relative_to(stage.resolve()):
                raise PackagingError("Bundle symlink escapes staging tree")
            continue
        sums.append(f"{hashlib.sha256(entry.read_bytes()).hexdigest()}  {entry.relative_to(stage).as_posix()}")
    (stage / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="utf-8")
    archive = output_dir / f"ritual-codec-{platform}-{kind}.zip"
    if archive.exists():
        archive.unlink()
    zip_stage(stage, archive)
    verify_archive(stage, archive)
    shutil.rmtree(stage)
    return archive


def _cache_enabled(cache_values: dict[str, str], kind: str) -> bool:
    return cache_values.get(f"RITUAL_BUILD_{kind.upper()}", "").upper() in {
        "ON", "TRUE", "YES", "1"
    }


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    root = Path(__file__).resolve().parents[1]
    output_dir = args.output_dir.resolve()
    if not build_dir.is_dir():
        raise PackagingError(f"build directory does not exist: {build_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    available = {
        "vst3": unique_bundles(build_dir, ".vst3"),
        "au": unique_bundles(build_dir, ".component"),
        "cli": [],
    }
    try:
        available["cli"] = find_cli_outputs(build_dir)
    except PackagingError:
        # A requested/default-enabled CLI reports the detailed failure below;
        # plugin-only packaging can proceed without CLI outputs.
        available["cli"] = []

    requested = args.kind
    if requested is None:
        cache_values = _read_cache_values(build_dir / "CMakeCache.txt")
        if cache_values:
            requested = [kind for kind in ("cli", "vst3", "au")
                         if _cache_enabled(cache_values, kind)]
        else:
            requested = [kind for kind in ("cli", "vst3", "au")
                         if available[kind]]
    if not requested:
        raise PackagingError("no enabled build outputs to package")
    archives = [make_archive(root, build_dir, output_dir, args.platform, kind,
                             available[kind]) for kind in dict.fromkeys(requested)]
    for archive in archives:
        print(archive)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, PackagingError, RuntimeError) as error:
        print(f"package_release.py: {error}", file=sys.stderr)
        raise SystemExit(2)
