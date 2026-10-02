# Manual de Usuario - Máquina Expendedora

Este documento describe el funcionamiento de la máquina expendedora, los productos disponibles, y cómo utilizar el teclado matricial (Keypad) y el lector RFID para realizar compras.

## 1. Productos Disponibles (Casillas)

La máquina cuenta con 4 casillas (canales) para productos. El estado inicial y la distribución de los productos es el siguiente:

| Casilla / Botón | Producto | Precio |
| :---: | :--- | :--- |
| **1** | Coca-Cola 355ml | $18.00 |
| **2** | Galletas Marias | $15.00 |
| **3** | Agua 600ml | $12.00 |
| **4** | Jugo Naranja | $14.00 |

*Nota: Estos precios y productos son los configurados por defecto y pueden ser modificados por el administrador de la máquina.*

---

## 2. Uso del Teclado Matricial (Keypad)

El teclado físico tiene un formato de 4x4 (estilo telefónico):

```
 1   2   3   A
 4   5   6   B
 7   8   9   C
 *   0   #   D
```

El funcionamiento de las teclas **cambia dependiendo de la pantalla (modo) actual** en la máquina.

### Pantalla de Inicio (Reposo)
En esta pantalla la máquina muestra los productos y sus precios alternándolos en un carrusel.
- **Teclas `1`, `2`, `3`, `4`**: Seleccionan el producto de la casilla correspondiente para comprarlo.

### Pantalla de Selección de Producto
- **Tecla `A`**: Confirmar el producto y pasar a elegir el método de pago.
- **Tecla `*`**: Cancelar y volver al inicio.

### Pantalla de Selección de Pago
Una vez seleccionado el producto, la máquina preguntará cómo deseas pagar.
- **Tecla `A`**: Elegir pagar con **Efectivo**.
- **Tecla `B`**: Elegir pagar con **Tarjeta RFID**.
- **Tecla `*`**: Cancelar la compra y volver al inicio.

### Pantalla de Efectivo
- **Teclas `1`, `2`, `3`, `4`**: Simulan la inserción de una moneda de $1, $2, $5 y $10.
- **Tecla `*`**: Cancelar y recuperar las monedas insertadas.

### Pantalla de Tarjeta RFID
- **Tecla `B` o `*`**: Cancelar sin cargo y volver al inicio.

### Pantallas de Mensajes y Alertas
- **Tecla `A`**: Botón de **Continuar** para descartar mensajes de alerta (ej. Producto Agotado o Errores).

Si no se pulsa ninguna tecla durante 3 minutos, la máquina cancela la operación y vuelve al inicio (devolviendo las monedas insertadas, si las hay).

---

## 3. Realizar una Compra

### Opción A: Comprar con Tarjeta RFID
1. En la pantalla de inicio, presiona el número de la casilla del producto deseado (ej. `1` para Coca-Cola).
2. La pantalla mostrará el método de pago. Presiona la tecla **`B`** (Tarjeta RFID).
3. La pantalla indicará: "Acerque su tarjeta al lector".
4. Acerca tu tarjeta MIFARE Classic al lector MFRC522.
5. El lector descontará el precio exacto directamente del saldo de la tarjeta.
6. Si tienes saldo suficiente, el producto será despachado inmediatamente.
7. *(Si deseas cancelar mientras esperas pasar la tarjeta, presiona la tecla **`*`**).*

*Nota Técnica del RFID: La máquina lee y descuenta el saldo del Sector 1, Bloque 4 de la tarjeta usando la clave de fábrica.*

### Opción B: Comprar con Efectivo
1. En la pantalla de inicio, presiona el número de la casilla del producto deseado (ej. `3` para Agua).
2. La pantalla mostrará el método de pago. Presiona la tecla **`A`** (Efectivo).
3. La pantalla mostrará lo insertado, lo que falta y las teclas de monedas. Utiliza las siguientes teclas para simular la inserción de cada denominación (las demás teclas se ignoran):
   - `1` = $1.00 peso
   - `2` = $2.00 pesos
   - `3` = $5.00 pesos
   - `4` = $10.00 pesos
4. Una vez que el saldo insertado alcance o supere el precio del producto, la máquina despachará el producto automáticamente y te entregará el cambio (una pantalla por denominación). Si la caja no puede formar el cambio exacto, se mostrará "No hay cambio" y el monto queda registrado como adeudo para el técnico.
5. *(Si deseas cancelar antes de completar el monto, presiona la tecla **`*`**. La máquina te devolverá las monedas insertadas).*

---
## 4. Fallos Mecánicos y Reembolsos
Si el motor no puede entregar el producto (barrera ocupada, atasco o producto no detectado), la máquina muestra "**Falla en proceso**" durante unos segundos y devuelve el dinero automáticamente, sin pulsar ninguna tecla:
- **Si pagaste con efectivo:** Se devuelven en monedas todas las monedas insertadas.
- **Si pagaste con RFID:** Se devuelve el monto cobrado a la tarjeta; **mantén la tarjeta sobre el lector** hasta que termine. Si no es posible escribir en la tarjeta, el monto se devuelve en monedas.

Si la caja no tiene monedas suficientes para el reembolso, se muestra "No hay monedas" y el monto queda registrado como adeudo para el técnico.
