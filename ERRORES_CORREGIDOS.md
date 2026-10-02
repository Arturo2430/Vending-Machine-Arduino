# Errores corregidos

Este documento registra el fallo del pago en efectivo, su causa y los cambios
realizados para resolverlo en el Arduino Mega 2560.

## 1. Las monedas simuladas no aumentaban el saldo

**Fecha:** 2 de octubre de 2026.  
**Estado:** corregido; funcionamiento confirmado por el usuario en la placa.

### Síntoma y comportamiento esperado

En el estado `S5_WAIT_CASH`, el LCD mostraba "Insertado: $0.00", aunque se
presionaran las teclas de monedas del teclado físico. Después de agregar
mensajes de diagnóstico, aparecía "Moneda no aceptada" y el monitor serie
mostraba:

```text
KEY=1 STATE=S5 ACTION=4
COIN_REJECTED CENTAVOS=1512
KEY=2 STATE=S5 ACTION=5
COIN_REJECTED CENTAVOS=224133120
KEY=3 STATE=S5 ACTION=6
COIN_REJECTED CENTAVOS=228527407
```

Las denominaciones correctas son:

| Tecla | Moneda simulada | Valor interno en centavos |
| :---: | --------------: | ------------------------: |
|  `1`  |           $1.00 |                       100 |
|  `2`  |           $2.00 |                       200 |
|  `3`  |           $5.00 |                       500 |
|  `4`  |          $10.00 |                      1000 |

El dinero se representa con `uint32_t`, un entero sin signo de 32 bits.
Trabajar en centavos permite sumar y comparar importes enteros.

### Lógica del pago

```text
Detectar una nueva pulsación del teclado.
Interpretar la tecla según el estado actual.
Si estamos en S5 y la tecla representa una moneda:
    Obtener su denominación en centavos.
    Intentar registrar una moneda de esa denominación en la caja.
    Si se acepta:
        Sumar la denominación al saldo insertado.
        Actualizar la pantalla o iniciar el despacho si se cubrió el precio.
    Si se rechaza:
        Mostrar el aviso y conservar el saldo anterior.
```

Los registros confirmaron que las teclas llegaban a S5 y se interpretaban
correctamente. El valor incorrecto aparecía al convertir la acción en dinero,
antes de intentar registrarlo en la caja.

### Causa confirmada

En `src/vm_keypad.cpp`, la tabla `COIN_VALUES` contenía los valores correctos,
pero se leía mediante un acceso normal de arreglo:

```cpp
static const uint32_t COIN_VALUES[4] = {
    100u, 200u, 500u, 1000u
};

// Dentro de coinActionToCentavos:
return COIN_VALUES[idx - base];
```

En `src/vm_eeprom_data.cpp` existía otra tabla idéntica,
`SEED_CASH_DENOMS`, declarada con `PROGMEM` para almacenarla en Flash.

Con LTO, la optimización que se realiza al enlazar los módulos, el compilador
fusionaba ambas tablas. En el firmware anterior, las dos tenían la dirección
de Flash `0x0262`. Sin embargo, el código del teclado utilizaba instrucciones
de lectura de RAM para acceder a esa dirección.

En el Mega, RAM y Flash son memorias separadas: leer la misma dirección
numérica en cada una no obtiene necesariamente el mismo dato. El teclado
terminaba leyendo datos ajenos a las denominaciones.

`VmEepromData::addCoins()` rechazaba esos importes porque la caja solo reconoce
100, 200, 500 y 1000 centavos. Por ello, nunca se ejecutaba la suma del saldo.
Los números incorrectos del registro son evidencia del fallo, no valores
fijos que deban reproducirse en todos los arranques.

### Corrección principal

Se hizo explícito tanto el almacenamiento en Flash como su lectura:

```cpp
#include <avr/pgmspace.h>

static const uint32_t COIN_VALUES[4] PROGMEM = {
    100u, 200u, 500u, 1000u
};

// Dentro de coinActionToCentavos:
return pgm_read_dword(&COIN_VALUES[idx - base]);
```

- `PROGMEM` coloca la tabla en la memoria de programa, Flash.
- `idx - base` convierte las acciones `SELECT_1` a `SELECT_4` en índices 0 a 3.
- `&` obtiene la dirección del elemento que se desea leer.
- `pgm_read_dword()` lee un valor de 32 bits desde Flash.
- `<avr/pgmspace.h>` proporciona estas herramientas para AVR.

Se conservó la validación que devuelve cero si la acción no representa una
moneda. Esta corrección mantiene los precios, las denominaciones y el formato
de datos de EEPROM existentes.

Referencia técnica: [atributo PROGMEM y lectura de Flash en GCC para AVR](https://gcc.gnu.org/onlinedocs/gcc-7.1.0/gcc/AVR-Variable-Attributes.html).

## 2. Ajuste adicional: pulsaciones perdidas al mantener otra tecla

Durante el diagnóstico se reprodujo un segundo problema: si `A` seguía
presionada al pulsar `4`, la lectura anterior podía ignorar la moneda.

En `src/main.cpp` se cambió `keypad.getKey()` por `keypad.getKeys()`. La primera
función solo devuelve una nueva pulsación si corresponde a la primera tecla
de la lista activa; la segunda actualiza la lista de teclas detectadas.

Ahora el bucle recorre esa lista y envía a la FSM únicamente los elementos
que cumplen ambas condiciones:

```cpp
keypad.key[i].stateChanged && keypad.key[i].kstate == PRESSED
```

`stateChanged` indica que la tecla cambió de estado y `PRESSED` representa
el comienzo de una pulsación. Así se detecta una moneda aunque otra tecla
siga activa, sin sumar repetidamente al mantenerla presionada.

Este ajuste resolvió una pérdida de entrada reproducible, pero el rechazo
con importes incorrectos se resolvió con la corrección de Flash del apartado 1.

## 3. Cambios de diagnóstico y documentación

| Archivo                                                              | Modificación                                                                                                                                                      |
| -------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [src/vm_keypad.cpp](src/vm_keypad.cpp)                               | Tabla de monedas en `PROGMEM` y lectura con `pgm_read_dword()`.                                                                                                   |
| [src/main.cpp](src/main.cpp)                                         | Lectura de nuevas pulsaciones mediante `getKeys()`.                                                                                                               |
| [src/vm_fsm.cpp](src/vm_fsm.cpp)                                     | Registros de tecla, estado, acción, moneda aceptada o rechazada y total; aviso de rechazo en el LCD; corrección de un comentario de cancelación para indicar `*`. |
| [include/vm_fsm.h](include/vm_fsm.h)                                 | Parámetro opcional `title` en `renderCashScreen()` para mostrar el aviso conservando los importes.                                                                |
| [test/avr_coin_values/main.cpp](test/avr_coin_values/main.cpp)       | Caso de regresión con una tabla idéntica en Flash, las cuatro monedas y una acción inválida.                                                                      |
| [test/avr_coin_values/run_test.py](test/avr_coin_values/run_test.py) | Compilación con AVR-GCC y LTO, ejecución en el simulador AVR-GDB y comprobación de resultados.                                                                    |
| [README.md](README.md)                                               | Instrucciones para ejecutar la prueba AVR y explicación de la lectura desde Flash.                                                                                |
| [docs/manual_usuario.md](docs/manual_usuario.md)                     | Secuencia correcta de selección y pago, comportamiento de pulsaciones y diagnóstico con el monitor serie.                                                         |

Los mensajes `KEY`, `STATE` y `ACTION` ayudan a localizar en qué paso ocurre
un fallo. `ACTION` es un código interno: por ejemplo, `ACTION=4` corresponde
a `SELECT_1`; no representa cuatro pesos.

## 4. Comprobaciones realizadas

- **Reproducción en AVR:** antes de la corrección, la prueba del simulador
  devolvía `COINS=0,0,0,0`, mientras la tabla de referencia leída desde Flash
  devolvía `100,200,500,1000`.
- **Prueba después de la corrección:** las cuatro monedas devolvieron los
  valores esperados y una acción inválida devolvió cero.
- **Firmware compilado:** se verificó que la lectura corregida utiliza
  instrucciones `LPM`, que leen Flash, en lugar de `LD/LDD`, que leen RAM.
- **Compilación PlatformIO:** completada correctamente.
- **Pruebas con hardware simulado:** acumulación, persistencia, cancelación,
  rechazo por caja llena y lectura de `4` mientras `A` sigue activa; mantener
  una tecla no acreditó monedas adicionales.
- **Prueba física:** el usuario cargó la corrección y confirmó que el pago
  en efectivo ya funcionaba.

La prueba inicial en la computadora no detectó el error de Flash porque ese
entorno no reproduce la separación de memorias del Mega. Por eso se añadió
la prueba específica para AVR.

### Cómo repetir la verificación

Desde la raíz del proyecto:

```powershell
python test/avr_coin_values/run_test.py
python -m platformio run
```

Para probar en la placa, cerrar primero cualquier monitor que use COM9:

```powershell
python -m platformio run -t upload --upload-port COM9
python -m platformio device monitor -p COM9 -b 115200
```

Pulsar y soltar `1`, `A`, `A`, `1`: seleccionar el canal 1, confirmar el
producto, elegir efectivo e insertar una moneda de $1.00. El resultado
esperado es "Insertado: $1.00" y:

```text
KEY=1 STATE=S5 ACTION=4
COIN_ACCEPTED CENTAVOS=100 TOTAL=100
```

En una compra nueva del canal 1, pulsar `1`, `2`, `3` en la pantalla de
efectivo debe acumular $1.00, $3.00 y $8.00. Pulsar `4` después completa
$18.00 y activa el despacho. Presionar `*` antes de cubrir el precio inicia
el reembolso del importe insertado.
