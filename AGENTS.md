# AGENTS.md

## Alcance

Estas instrucciones aplican a todo el proyecto `Vending-Machine-Arduino`, un
firmware escrito en C++ con PlatformIO para Arduino Mega 2560. La máquina
expendedora se programa mediante una máquina de estados finitos (FSM), que
coordina la lógica de negocio y se ejecuta en un único microcontrolador.

## Tecnología y modelo de control

- Lenguaje principal: C++ para el framework Arduino.
- Modelo de control: máquina de estados finitos (FSM) implementada en
  `vm_fsm.*`.
- Cada estado representa una fase de la máquina expendedora, como reposo,
  selección, pago, despacho, cálculo de cambio o administración.
- Las entradas del teclado, RFID y sensores producen eventos que provocan
  transiciones entre estados.

## Estructura

- `src/`: implementaciones C++ del firmware y `main.cpp`.
- `include/`: cabeceras compartidas y configuración de placa.
- `platformio.ini`: entorno `megaatmega2560`, framework Arduino y dependencias.
- `docs/`: manual de usuario y documentación técnica.
- `README.md`: arquitectura, hardware, estados y datos semilla.

Módulos principales:

- `vm_fsm.*`: estados y transiciones de compra y administración.
- `vm_eeprom_data.*`: persistencia de productos, caja, PIN y última orden.
- `vm_motor_controller.*` y `vm_motor_config.h`: despacho mediante PCA9685.
- `vm_keypad.*`, `vm_display.*` y `vm_carousel.*`: entrada y presentación.
- `vm_change_calculator.*`: cálculo del cambio.
- `vm_rfid.*`: pago y lectura RFID.

## Comandos

Ejecutar desde la raíz del proyecto:

```bash
pio run                         # compilar
pio run -t upload               # cargar al Arduino por USB
pio device monitor -b 115200    # abrir el monitor serie
```

El puerto de carga puede especificarse cuando sea necesario:

```bash
pio run -t upload --upload-port /dev/ttyACM0
```

## Convenciones de implementación

- Mantener la separación entre la FSM, los drivers de hardware y la
  persistencia. Las transiciones deben quedar centralizadas en `vm_fsm.*`.
- Usar los tipos y constantes existentes en `vm_types.h` y en los archivos de
  configuración; evitar números de pines o límites de hardware duplicados.
- Representar dinero en centavos (`uint32_t`) y conservar las unidades actuales
  al modificar cálculos, mensajes o persistencia.
- Respetar las interfaces públicas de los módulos y el estilo C++ ya existente.
- Tener en cuenta la memoria y el tiempo de ejecución limitados del AVR: evitar
  asignaciones dinámicas innecesarias y operaciones bloqueantes en el bucle
  principal.
- Las operaciones de motores deben seguir el patrón no bloqueante
  `start/poll/stop`; no introducir esperas largas dentro de `loop()` o de la
  FSM.
- Revisar cuidadosamente los cambios que escriban EEPROM para no alterar el
  formato persistido sin una estrategia de compatibilidad o inicialización.
- Comentarios en espanol sin acentos, estos deben ser concisos.
- Variables en ingles.

## Flujo de cambios

1. Leer `README.md`, `platformio.ini` y el módulo afectado antes de editar.
2. Mantener los cambios enfocados y actualizar la documentación si cambia el
   hardware, el protocolo de uso o la FSM.
3. Compilar con `pio run` después de cada cambio relevante.

## Git
No crear commits salvo que se solicite explícitamente.

- Todos los mensajes de commit deben escribirse en español, con la excepción estricta de los prefijos de «Conventional Commit» (por ejemplo, `feat:`, `docs:`, `refactor:`, `fix:`, etc.), que deben mantenerse en inglés. 
- Usar la identidad de quien realiza el trabajo, sin atribuciones de asistentes.
- El mensaje final al usuario debe ser breve: resultado, archivos o evidencia esenciales, comprobaciones realizadas y pendientes o límites que afecten el uso. Omitir narración del proceso, listas exhaustivas de archivos y detalles operativos sin relevancia.