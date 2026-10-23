# PlatformIO pre-build script: refuses to build a filesystem image from a theme
# folder with no sounds in it. mklittlefs reports success on an empty directory, so
# without this an uploadfs flashes a blank filesystem over the sounds and the history
# ring, exits 0, and the first hint of trouble is a silent board.
import os

Import("env")

FS_TARGETS = ("buildfs", "uploadfs", "uploadfsota")
SOUNDS = (
    "5h-warning.wav", "5h-depleted.wav", "5h-reset.wav",
    "7d-warning.wav", "7d-depleted.wav", "7d-reset.wav",
)

if not set(FS_TARGETS) & set(COMMAND_LINE_TARGETS):
    Return()

data_dir = env.subst("$PROJECT_DATA_DIR")
present = set(os.listdir(data_dir)) if os.path.isdir(data_dir) else set()
missing = [name for name in SOUNDS if name not in present]

if len(missing) == len(SOUNDS):
    print("\nNo sounds in '%s'." % data_dir)
    print("data_dir in platformio.ini must name a theme folder holding %s." % ", ".join(SOUNDS))
    print("The WAVs are gitignored, so a fresh clone has to supply them:")
    print("see the \"Preparing audio files\" section of docs/ALERT_SOUNDS.md.")
    env.Exit(1)

if missing:
    print("\nWarning: '%s' is missing %s." % (data_dir, ", ".join(missing)))
    print("Those alerts will be silent. Flashing the rest.\n")
