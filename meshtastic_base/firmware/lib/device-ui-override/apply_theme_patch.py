"""
Pre-build script to apply Unbound Meshberry patches to device-ui library.
This copies our patched files over the library's versions before compilation.

Patched files:
- Themes.cpp: Color palette (Emerald accent, dark backgrounds)
- screens.c: Boot screen branding (UNBOUND logo, dark background)

NOTE: EncoderInputDriver.cpp was removed from patch list - the RISING->FALLING
change was unnecessary (trackball direction already works, menu opening issue
is unrelated to interrupt edge detection).
"""
import os
import shutil

Import("env")

# List of files to patch: (source_relative_path, target_relative_path)
PATCH_FILES = [
    # Themes.cpp - color palette
    ("source/graphics/TFT/Themes.cpp", "source/graphics/TFT/Themes.cpp"),
    # screens.c - boot screen branding
    ("generated/ui_320x240/screens.c", "generated/ui_320x240/screens.c"),
]

def apply_patches(source, target, env):
    """Copy all patched files to device-ui library location."""

    project_dir = env.get("PROJECT_DIR", "")
    libdeps_dir = os.path.join(project_dir, ".pio", "libdeps", env.get("PIOENV", ""))
    override_dir = os.path.join(project_dir, "lib", "device-ui-override")

    # Find the device-ui library directory
    device_ui_dir = None
    if os.path.exists(libdeps_dir):
        for name in os.listdir(libdeps_dir):
            if "device-ui" in name.lower() or "meshtastic-device-ui" in name.lower():
                device_ui_dir = os.path.join(libdeps_dir, name)
                break

    if device_ui_dir is None:
        print("WARNING: device-ui library not found, Unbound Meshberry patches not applied")
        return

    # Apply each patch
    for source_rel, target_rel in PATCH_FILES:
        patch_source = os.path.join(override_dir, source_rel)
        patch_target = os.path.join(device_ui_dir, target_rel)

        if not os.path.exists(patch_source):
            # Skip if patch file doesn't exist (optional patches)
            continue

        if os.path.exists(patch_target):
            # Backup original if not already backed up
            backup_path = patch_target + ".original"
            if not os.path.exists(backup_path):
                shutil.copy2(patch_target, backup_path)
                print(f"Backed up original {os.path.basename(patch_target)}")

            # Copy our patched version
            shutil.copy2(patch_source, patch_target)
            print(f"Applied Unbound Meshberry patch: {os.path.basename(patch_target)}")
        else:
            print(f"WARNING: Target not found: {patch_target}")

# Apply patches immediately during script load (before compilation)
# The pre: prefix ensures this script runs after library resolution but before compile
apply_patches(None, None, env)
