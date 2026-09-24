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
- **Tecla `A`**: Permite ingresar al Menú de Administrador (requiere NIP de 4 dígitos, por defecto `1234`).

### Pantalla de Selección de Pago
Una vez seleccionado el producto, la máquina preguntará cómo deseas pagar.
- **Tecla `A`**: Elegir pagar con **Efectivo**.
- **Tecla `B`**: Elegir pagar con **Tarjeta RFID**.
- **Tecla `*`**: Cancelar la compra y volver al inicio.

---

## 3. Realizar una Compra

### Opción A: Comprar con Tarjeta RFID
1. En la pantalla de inicio, presiona el número de la casilla del producto deseado (ej. `1` para Coca-Cola).
2. La pantalla mostrará el método de pago. Presiona la tecla **`B`** (Tarjeta RFID).
3. La pantalla indicará: "Acerque su tarjeta al lector".
4. Acerca tu tarjeta MIFARE Classic al lector MFRC522.
5. El lector descontará el precio exacto directamente del saldo de la tarjeta.
6. Si tienes saldo suficiente, el producto será despachado inmediatamente.
7. *(Si deseas cancelar mientras esperas pasar la tarjeta, presiona la tecla **`B`**).*

*Nota Técnica del RFID: La máquina lee y descuenta el saldo del Sector 1, Bloque 4 de la tarjeta usando la clave de fábrica.*

### Opción B: Comprar con Efectivo
1. En la pantalla de inicio, presiona el número de la casilla del producto deseado (ej. `3` para Agua).
2. La pantalla mostrará el método de pago. Presiona la tecla **`A`** (Efectivo).
3. La pantalla mostrará "Inserta monedas". Utiliza las siguientes teclas numéricas para simular la inserción de diferentes denominaciones:
   - `1` = $1.00 peso
   - `2` = $2.00 pesos
   - `3` = $5.00 pesos
   - `4` = $10.00 pesos
   - `5` = $20.00 pesos
   - `6` = $50.00 pesos
   - `7` = $100.00 pesos
   - `8` = $200.00 pesos
   - `9` = $500.00 pesos
4. Una vez que el saldo insertado alcance o supere el precio del producto, la máquina despachará el producto automáticamente y te calculará tu cambio.
5. *(Si deseas cancelar e interrumpir la compra antes de completar el monto, presiona la tecla **`B`**).*
