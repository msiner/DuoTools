

import os.path
import shutil


API_DIR = os.path.join('C:\\', 'Program Files', 'SDRPlay', 'API')
DLL_X86 = os.path.join(API_DIR, 'x86', 'sdrplay_api.dll')
DLL_X64 = os.path.join(API_DIR, 'x64', 'sdrplay_api.dll')

TARGETS = [
    ('Debug', DLL_X86),
    ('Release', DLL_X86),
    (os.path.join('x64', 'Debug'), DLL_X64),
    (os.path.join('x64', 'Release'), DLL_X64)]


def main():
    for target_dir, dll_path in TARGETS:
        if os.path.isdir(target_dir):
            print('Found target directory: %s' % target_dir)
            if os.path.isfile(dll_path):
                dll_name = os.path.basename(dll_path)
                inst_path = os.path.join(target_dir, dll_name)
                print('Installing DLL: %s -> %s' % (dll_path, inst_path))
                shutil.copyfile(dll_path, inst_path)
            else:
                print('DLL unavailable: %s' % dll_path)


if __name__ == '__main__':
    main()