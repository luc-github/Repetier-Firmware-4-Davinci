from os.path import join, isfile
from shutil import copyfile


Import("env")
# access to global construction environment
ROOT_DIR = env['PROJECT_DIR']
FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduino-sam")
patchflag_path = join(FRAMEWORK_DIR, ".patching-done")

# patch file only if we didn't do it before
if not isfile(join(FRAMEWORK_DIR, ".patching-done")):
    print("Patching files")
    target_file = join(FRAMEWORK_DIR, "cores","arduino","USB", "USBCore.cpp")
    patched_file = join(ROOT_DIR, "PIOPatches", "USBCore.cpp")
    assert isfile(target_file) and isfile(patched_file)
    copyfile(patched_file, target_file)
    target_file = join(FRAMEWORK_DIR, "variants", "arduino_due_x", "variant.cpp")
    patched_file = join(ROOT_DIR, "PIOPatches", "variant.cpp")
    assert isfile(target_file) and isfile(patched_file)
    copyfile(patched_file, target_file)

    def _touch(path):
        with open(path, "w") as fp:
            fp.write("")

    env.Execute(lambda *args, **kwargs: _touch(patchflag_path))
else:
    print("Files already patched")