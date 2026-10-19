"""Build, verify, install, and launch the local macOS Neden app."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time

from check_playback_licenses import REVIEWED_FRAMEWORKS

ROOT = Path(__file__).resolve().parent.parent
BUNDLE_ID = "io.github.hanifcarroll.neden.qt-lab"
MACHO_HEADERS = {b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
                 b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca"}


def run(*arguments):
    subprocess.run([str(argument) for argument in arguments], check=True)


def binaries(bundle):
    found = set()
    for path in bundle.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as file:
            if file.read(4) in MACHO_HEADERS:
                found.add(path)
    return sorted(found)


def local_signing_identity():
    cache = (ROOT / "build/desktop/CMakeCache.txt").read_text()
    for line in cache.splitlines():
        if line.startswith("NEDEN_SIGNING_IDENTITY:STRING="):
            return line.split("=", 1)[1]
    raise SystemExit("No persistent signing identity in the configured desktop build")


def stage_module(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    for item in sorted(source.iterdir()):
        target = destination / item.name
        if item.is_dir():
            if not (item / "qmldir").is_file():
                stage_module(item, target)
        elif item.is_file():
            shutil.copy2(item, target)


def stage_native_qml(bundle):

    # 1. Ask Qt which imports the app uses, preserving their normal directory layout.
    qtpaths = os.environ.get("NEDEN_QTPATHS", shutil.which("qtpaths"))
    if not qtpaths:
        raise SystemExit("qtpaths is required to locate this Qt installation")
    def qt_directory(name):
        return Path(subprocess.check_output([qtpaths, "--query", name], text=True).strip())

    sdk = qt_directory("QT_INSTALL_QML")
    scanner = qt_directory("QT_INSTALL_LIBEXECS") / "qmlimportscanner"
    rows = json.loads(subprocess.check_output([str(scanner), "-rootPath", str(ROOT / "apps/desktop/ui"),
                                              "-importPath", str(sdk)], text=True))
    qml = bundle / "Contents/Resources/qml"
    for row in rows:
        name = row.get("name", "")
        if name.startswith(("QtWebEngine", "QtWebChannel")) or not row.get("path"):
            continue
        source = Path(row["path"])
        if row.get("type") == "module" and source.is_relative_to(sdk):
            stage_module(source, qml / source.relative_to(sdk))

    # 2. Select the Mac's WKWebView backend instead of the SDK's optional Chromium imports.
    metadata = qml / "QtWebView/qmldir"
    text = metadata.read_text()
    optional_dependency = "depends QtWebEngine auto\n"
    if text.count(optional_dependency) > 1:
        raise SystemExit("QtWebView metadata changed; review its native deployment")
    metadata.write_text(text.replace(optional_dependency, ""))
    plugins = list(qml.rglob("*.dylib"))
    for relative in ("platforms/libqcocoa.dylib", "webview/libqtwebview_darwin.dylib",
                     "tls/libqsecuretransportbackend.dylib", "imageformats/libqsvg.dylib"):
        source = qt_directory("QT_INSTALL_PLUGINS") / relative
        destination = bundle / "Contents/PlugIns" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        plugins.append(destination)
    return plugins, qt_directory("QT_INSTALL_LIBS")


def prepare_bundle(destination):

    # 1. Copy only the selected app and its reviewed playback frameworks into staging.
    source = ROOT / "build/desktop/Neden.app"
    run("ditto", source, destination)
    runtime = Path(os.environ.get("NEDEN_MPV_FRAMEWORKS", ROOT / ".cache/playback-frameworks")).resolve()
    run(sys.executable, ROOT / "scripts/check_playback_licenses.py", runtime)
    frameworks = destination / "Contents/Frameworks"
    frameworks.mkdir(exist_ok=True)
    for name in sorted(REVIEWED_FRAMEWORKS):
        run("ditto", runtime / f"{name}.framework", frameworks / f"{name}.framework")

    # 2. Let Qt collect its QML imports, platform plugins, and dynamic dependencies.
    deploy = os.environ.get("NEDEN_MACDEPLOYQT", shutil.which("macdeployqt"))
    if not deploy:
        raise SystemExit("macdeployqt is required to prepare the installed app")
    plugins, qt_libraries = stage_native_qml(destination)
    run(deploy, destination, "-no-plugins", "-no-codesign", "-no-strip", f"-libpath={qt_libraries}",
        f"-libpath={runtime}", *[f"-executable={plugin}" for plugin in plugins])
    (destination / "Contents/Resources/qt.conf").write_text(
        "[Paths]\nPrefix=.\nPlugins=PlugIns\nQmlImports=Resources/qml\nLibraries=Frameworks\n")
    files = binaries(destination)
    for binary in files:
        load_commands = subprocess.check_output(["otool", "-l", str(binary)], text=True).splitlines()
        if any(line.strip() == "cmd LC_ID_DYLIB" for line in load_commands):
            library_id = "@rpath/" + str(binary.relative_to(frameworks)) if binary.is_relative_to(frameworks) \
                else "@loader_path/" + binary.name
            run("install_name_tool", "-id", library_id, binary)
        for index, line in enumerate(load_commands):
            if line.strip() != "cmd LC_RPATH":
                continue
            path = load_commands[index + 2].strip().split(" (offset ")[0].removeprefix("path ")
            if path.startswith("/"):
                run("install_name_tool", "-delete_rpath", path, binary)
        linkage = subprocess.check_output(["otool", "-L", str(binary)], text=True)
        for line in linkage.splitlines():
            if not line.startswith("\t"):
                continue
            dependency = line.strip().split(" (compatibility version ")[0]
            if dependency.startswith("/") and not dependency.startswith(("/System/Library/", "/usr/lib/")):
                parts = Path(dependency).parts
                framework_index = next((index for index, part in enumerate(parts) if part.endswith(".framework")), None)
                relative = Path(*parts[framework_index:]) if framework_index is not None else Path(dependency).name
                if not (frameworks / relative).exists():
                    raise SystemExit(f"Unbundled library dependency in {binary.name}: {dependency}")
                replacement = "@rpath/" + str(relative)
                run("install_name_tool", "-change", dependency, replacement, binary)
                dependency = replacement
            if dependency.startswith("@rpath/") and not (frameworks / dependency.removeprefix("@rpath/")).exists():
                raise SystemExit(f"Missing bundled dependency in {binary.name}: {dependency}")
            if dependency.startswith("@loader_path/") and not (binary.parent / dependency.removeprefix("@loader_path/")).exists():
                raise SystemExit(f"Missing loader-relative dependency in {binary.name}: {dependency}")

    # 3. Preserve the established Keychain identity after all binary edits are complete.
    info_path = destination / "Contents/Info.plist"
    info = plistlib.loads(info_path.read_bytes())
    revision = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT):
        revision += "-dirty"
    info.update(CFBundleIdentifier=BUNDLE_ID, CFBundleName="Neden", CFBundleDisplayName="Neden", CFBundleExecutable="Neden",
                NedenSourceRevision=revision)
    info_path.write_bytes(plistlib.dumps(info))
    notices = destination / "Contents/Resources/licenses"
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "LICENSE", notices / "Neden-MIT.txt")
    shutil.copy2(ROOT / "shared/fonts/LICENSE.txt", notices / "Inter-OFL.txt")
    shutil.copy2(ROOT / "docs/desktop/DEPENDENCIES.md", notices / "Dependencies.md")
    shutil.copy2(ROOT / "shared/icons/lucide/LICENSE", notices / "Lucide-ISC.txt")
    identity = local_signing_identity()
    if not identity or identity == "-":
        raise SystemExit("A persistent Apple Development identity is required")
    for binary in files:
        run("codesign", "--force", "--sign", identity, "--timestamp=none", binary)
    for framework in sorted(frameworks.glob("*.framework")):
        run("codesign", "--force", "--sign", identity, "--timestamp=none", framework)
    run("codesign", "--force", "--sign", identity, "--identifier", BUNDLE_ID, "--timestamp=none", destination)
    run("codesign", "--verify", "--deep", "--strict", destination)
    run(sys.executable, ROOT / "scripts/check_playback_licenses.py", frameworks)
    return revision


def running_pids(bundle):
    executable = str(bundle / "Contents/MacOS/Neden")
    rows = subprocess.check_output(["ps", "-axo", "pid=,comm="], text=True).splitlines()
    return [int(row.strip().split(None, 1)[0]) for row in rows
            if len(row.strip().split(None, 1)) == 2 and row.strip().split(None, 1)[1] == executable]


def stop_installed_app(bundle):
    if not running_pids(bundle):
        return

    # Ask the app to quit normally so its final playback position reaches disk.
    script = """ObjC.import('AppKit');
function run(arguments) {
    const app = $.NSRunningApplication.runningApplicationWithProcessIdentifier(Number(arguments[0]));
    if (app) app.terminate;
}"""
    for pid in running_pids(bundle):
        run("osascript", "-l", "JavaScript", "-e", script, str(pid))
    deadline = time.monotonic() + 15
    while running_pids(bundle) and time.monotonic() < deadline:
        time.sleep(0.2)
    if running_pids(bundle):
        raise SystemExit("Neden did not quit normally; the installed app was not replaced")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-launch", action="store_true", help="Install without opening the app")
    args = parser.parse_args()
    if sys.platform != "darwin":
        raise SystemExit("This installer is for macOS")

    # 1. Build and run the checks before touching the installed version.
    run("sh", ROOT / "scripts/desktop.sh", "test")
    run("cmake", "--build", ROOT / "build/desktop", "--target", "NedenQt_qmllint")
    destination = Path("/Applications/Neden.app")
    with tempfile.TemporaryDirectory(prefix="neden-install-", dir=ROOT / ".cache") as staging:
        staged = Path(staging) / "Neden.app"
        revision = prepare_bundle(staged)

        # 2. Stage on the destination volume and retain the old app until replacement succeeds.
        with tempfile.TemporaryDirectory(prefix=".neden-update-", dir=destination.parent) as update:
            replacement = Path(update) / "Neden.app"
            backup = Path(update) / "Previous.app"
            run("ditto", staged, replacement)
            run("codesign", "--verify", "--deep", "--strict", replacement)
            stop_installed_app(destination)
            if destination.exists():
                installed_info = plistlib.loads((destination / "Contents/Info.plist").read_bytes())
                if installed_info.get("CFBundleIdentifier") != BUNDLE_ID:
                    raise SystemExit("Another application's bundle occupies /Applications/Neden.app")
                destination.rename(backup)
            try:
                replacement.rename(destination)
                run("codesign", "--verify", "--deep", "--strict", destination)
                register = "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
                run(register, "-f", destination)
                if not args.no_launch:
                    run("open", destination)
                    deadline = time.monotonic() + 10
                    while not running_pids(destination) and time.monotonic() < deadline:
                        time.sleep(0.2)
                    time.sleep(1)
                    if not running_pids(destination):
                        raise RuntimeError("The installed app did not stay running")
            except BaseException:
                if destination.exists():
                    stop_installed_app(destination)
                    shutil.rmtree(destination)
                if backup.exists():
                    backup.rename(destination)
                raise
    print(f"Installed {destination} from source {revision} with the persistent signing identity.")


if __name__ == "__main__":
    main()
