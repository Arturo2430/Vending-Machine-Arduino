"""Comprueba la conversion de monedas con AVR-GCC y el simulador de AVR-GDB."""

from pathlib import Path
import re
import shutil
import subprocess
import sys


root = Path(__file__).resolve().parents[2]
toolchain = Path.home() / '.platformio' / 'packages' / 'toolchain-atmelavr' / 'bin'


def find_tool(name):
    found = shutil.which(name)
    if found:
        return found
    bundled = toolchain / (name + ('.exe' if sys.platform == 'win32' else ''))
    if bundled.is_file():
        return str(bundled)
    raise SystemExit(f'No se encontro {name}; instala el entorno con python -m platformio run.')


work = root / '.pio' / 'avr-coin-test'
work.mkdir(parents=True, exist_ok=True)
elf = work / 'coins.elf'
# Una ruta opcional permite comprobar la version anterior sin editar el proyecto.
source = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'src' / 'vm_keypad.cpp'
subprocess.run([
    find_tool('avr-g++'), '-mmcu=atmega2560', '-std=gnu++11', '-Os', '-flto', '-g',
    '-I', str(root / 'include'), str(source), str(Path(__file__).with_name('main.cpp')),
    '-o', str(elf),
], check=True)

commands = work / 'check.gdb'
commands.write_text('''set pagination off
target sim
load
break testComplete
run
printf "COINS=%lu,%lu,%lu,%lu\\n", (unsigned long)coinValues[0], (unsigned long)coinValues[1], (unsigned long)coinValues[2], (unsigned long)coinValues[3]
printf "REFERENCE=%lu,%lu,%lu,%lu\\n", (unsigned long)referenceValues[0], (unsigned long)referenceValues[1], (unsigned long)referenceValues[2], (unsigned long)referenceValues[3]
printf "INVALID=%lu\\n", (unsigned long)invalidValue
quit
''', encoding='utf-8')
result = subprocess.run([
    find_tool('avr-gdb'), '-batch', str(elf), '-x', str(commands),
], capture_output=True, text=True, timeout=30)
output = result.stdout + result.stderr
expected = (100, 200, 500, 1000)
for label in ('COINS', 'REFERENCE'):
    match = re.search(rf'{label}=(\d+),(\d+),(\d+),(\d+)', output)
    if not match or tuple(map(int, match.groups())) != expected:
        print(output)
        raise SystemExit(f'FAIL: {label} debe ser {expected}.')
if result.returncode or not re.search(r'INVALID=0\b', output):
    print(output)
    raise SystemExit('FAIL: una accion sin moneda debe devolver cero.')
print('PASS: AVR con LTO devuelve 100/200/500/1000 centavos; accion invalida devuelve 0.')
