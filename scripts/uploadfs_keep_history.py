"""Flash a sound theme to LittleFS without losing the usage history ring.

Run: python scripts/uploadfs_keep_history.py [--theme sounds/jarvis] [--port COM5]

A plain `pio run -t uploadfs` replaces the whole filesystem partition, and
/history.bin lives there beside the WAVs, so swapping themes costs the 7-day
history. This reads the partition off the board first, carries every non-WAV
file over into the new image, and flashes that.

Needs littlefs-python (`pip install littlefs-python`) to read the dump.
"""

import argparse
import configparser
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# A 64 KB chunk reads in about a second. Reading the whole 1.5 MB in one esptool
# call fails regularly with "Serial data stream stopped" over USB-Serial/JTAG, at a
# different offset each time, so each chunk is read separately and retried.
CHUNK = 64 * 1024
TRIES = 6

PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
PARTITION_MAGIC = 0x50AA
TYPE_DATA = 0x01
SUBTYPE_SPIFFS = 0x82

# esp_littlefs formats the partition with 4 KB blocks
LFS_BLOCK_SIZE = 4096


def core_dir():
    return Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))


def esptool_command():
    core = core_dir()
    script = core / "packages" / "tool-esptoolpy" / "esptool.py"
    python = core / "penv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if not script.is_file():
        sys.exit("esptool.py not found under %s - run a `pio run` once to install it." % core)
    return [str(python if python.is_file() else sys.executable), str(script)]


def run_esptool(args, port, log_path):
    """Run esptool with its output on a file. A pipe can deadlock mid-transfer."""
    command = esptool_command() + ["--chip", "esp32s3"]
    if port:
        command += ["--port", port]
    # Leave the chip in the bootloader: a running firmware would append a history
    # sample after the dump, and that sample would be lost by the flash that follows.
    command += ["--after", "no-reset"] + args
    environment = {**os.environ, "PYTHONUTF8": "1", "PYTHONIOENCODING": "utf-8"}
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        return subprocess.call(command, stdout=log, stderr=subprocess.STDOUT, env=environment)


def find_fs_partition(port, work):
    """Read the partition table off the board and locate the filesystem partition."""
    table = work / "partitions.bin"
    if run_esptool(["read-flash", hex(PARTITION_TABLE_OFFSET), hex(PARTITION_TABLE_SIZE),
                    str(table)], port, work / "partitions.log") != 0:
        sys.exit("Could not read the partition table:\n%s" % (work / "partitions.log").read_text())

    blob = table.read_bytes()
    for position in range(0, len(blob), 32):
        entry = blob[position:position + 32]
        if len(entry) < 32:
            break
        magic, kind, subtype, offset, size = struct.unpack("<HBBII", entry[:12])
        if magic != PARTITION_MAGIC:
            break
        if kind == TYPE_DATA and subtype == SUBTYPE_SPIFFS:
            label = entry[12:28].rstrip(b"\x00").decode("utf-8", "replace")
            return offset, size, label
    sys.exit("No filesystem partition in the table - nothing to preserve.")


def dump_partition(port, offset, size, work):
    """Read the partition in retried chunks; returns the assembled bytes."""
    chunks = work / "chunks"
    chunks.mkdir()
    blob = bytearray()
    for index, start in enumerate(range(0, size, CHUNK)):
        length = min(CHUNK, size - start)
        piece = chunks / ("%06x.bin" % start)
        log = chunks / ("%06x.log" % start)
        for attempt in range(1, TRIES + 1):
            code = run_esptool(["read-flash", hex(offset + start), hex(length), str(piece)],
                               port, log)
            if code == 0 and piece.is_file() and piece.stat().st_size == length:
                break
            piece.unlink(missing_ok=True)
            print("  chunk %d/%d failed (attempt %d of %d)"
                  % (index + 1, (size + CHUNK - 1) // CHUNK, attempt, TRIES))
        else:
            sys.exit("Gave up reading at %s. Nothing was written to the board.\n%s"
                     % (hex(offset + start), log.read_text()))
        blob += piece.read_bytes()
        print("  read %d/%d KB" % (len(blob) // 1024, size // 1024), end="\r")
    print()
    return bytes(blob)


def littlefs_class():
    """Checked before the dump: a missing dependency would waste a minute of reading."""
    try:
        from littlefs import LittleFS
    except ImportError:
        sys.exit("littlefs-python is needed to read the dump: pip install littlefs-python")
    return LittleFS


def extract_keepers(blob, destination):
    """Copy every non-WAV file out of the dump. Returns the names kept."""
    LittleFS = littlefs_class()
    fs = LittleFS(block_size=LFS_BLOCK_SIZE, block_count=len(blob) // LFS_BLOCK_SIZE, mount=False)
    fs.context.buffer = bytearray(blob)
    try:
        fs.mount()
    except Exception as error:
        # A board that has never had an uploadfs has nothing to preserve
        print("  no readable filesystem (%s) - flashing the theme alone" % error)
        return []

    kept = []
    for folder, _, files in fs.walk("/"):
        for name in files:
            path = folder.rstrip("/") + "/" + name
            if name.lower().endswith(".wav"):
                continue
            with fs.open(path, "rb") as handle:
                payload = handle.read()
            target = destination / path.lstrip("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(payload)
            kept.append("%s (%d bytes)" % (path, len(payload)))
    return kept


def project_theme():
    """The data_dir from platformio.ini, which is the theme uploadfs would flash."""
    parser = configparser.ConfigParser()
    parser.read(ROOT / "platformio.ini", encoding="utf-8")
    return parser.get("platformio", "data_dir", fallback="data")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--theme", help="theme folder to flash (default: data_dir in platformio.ini)")
    parser.add_argument("--port", help="serial port (default: let esptool detect it)")
    arguments = parser.parse_args()

    theme = Path(arguments.theme or project_theme())
    if not theme.is_absolute():
        theme = ROOT / theme
    wavs = sorted(theme.glob("*.wav"))
    if not wavs:
        sys.exit("No WAVs in %s - nothing to flash." % theme)
    littlefs_class()

    work = Path(tempfile.mkdtemp(prefix="keep-history-"))
    try:
        print("Reading the partition table...")
        offset, size, label = find_fs_partition(arguments.port, work)
        print("  '%s' partition at %s, %d KB" % (label, hex(offset), size // 1024))

        print("Dumping the filesystem (the board stays in its bootloader)...")
        blob = dump_partition(arguments.port, offset, size, work)

        stage = work / "stage"
        stage.mkdir()
        print("Preserving non-WAV files...")
        kept = extract_keepers(blob, stage)
        for item in kept:
            print("  kept %s" % item)

        for wav in wavs:
            shutil.copy2(wav, stage / wav.name)
        print("Flashing %d WAVs from %s plus %d preserved file(s)..."
              % (len(wavs), theme, len(kept)))

        # UTF8 vars: esptool v5 draws a Unicode progress bar that kills pio's writer on
        # cp1252, orphaning an esptool that holds the port. OFFLINE skips the penv
        # dependency re-install, which fails when it cannot reach the network.
        environment = {**os.environ, "PLATFORMIO_DATA_DIR": str(stage),
                       "PYTHONUTF8": "1", "PYTHONIOENCODING": "utf-8"}
        environment.setdefault("PLATFORMIO_OFFLINE", "1")
        if arguments.port:
            environment["PLATFORMIO_UPLOAD_PORT"] = arguments.port

        # Output goes straight to the terminal: piping an upload can deadlock esptool.
        return subprocess.call(
            [str(core_dir() / "penv" / ("Scripts/pio.exe" if os.name == "nt" else "bin/pio")),
             "run", "-t", "uploadfs"],
            cwd=ROOT,
            env=environment,
        )
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
