# Instrucciones Repositorio CEAJarillonCali

En este leeme se cubrirán instrucciones básicas para operar el repositorio, es decir, las modificaciones necesarias para compilar y subir el programa a las RE-Motes de Zolertia

# Nota:
Se asume que el usuario sabe operar el SO Contiki junto con sus procesos.

# Envio de señales entre procesos

En el caso de este repositorio se está trabajando con 3 procesos distintos, con diferentes tiempos de muestreo. Dado que los procesos envian señales a la función maestra que controla la FSM se hace necesario un mecanismo que permita compartir las señales que luego van a ser publicadas al Broker. Para eso se dispone de la estructura **"signal_tag_tx"** ubicada en el archivo **"add-on.h"** linea **127**. Los nombres que se le dió pueden ser cambiados sin ningún problema, lo que hay que tener especial cuidado es el tipo de los datos, en el caso del campo que pertenece a los valores el tipo es de 16bit sin signo, si se desea cambiar por uno de 32bit o aumentar el tamaño del nombre de la variable se deben cambiar ciertos valores que se tratarán en el apartado de **Publicación** más abajo.

**Nota1:** Tener en cuenta que la estructura **"signal_tag_tx"** puede tener entre 1 y N pares de variables a almacenar.

**Nota2:** Incrementar la cantidad de pares de variables (Nombre y Valor) dentro de la estructura no requiere de más modificaciones por parte del desarrollador.
**Las modificaciones son requeridas si el desarrollador decide cambiar el tipo de datos que están definidas acutalmente en la estrucura, por tanto no se recomienda a menos que tenga conocimiento de punteros.**

# Publicación al Broker

El hilo **mqtt_demo_process** en la línea **214** del archivo **node1.c** es el encargado de la configuración, envío y recepción de datos mediante el radio y manejo de la FSM, por tanto se procederá a realizar una breve explicación de la manipulación de este hilo para realizar el proceso de publicación.

- El proceso de configuración de la FSM se ubica en las líneas **214 - 225** y por tanto **NO deben ser alterados!!**. Las líneas **229 - 231** se encargan de dar el nombre al tópico que se desea publicar en el broker por cada una de las señales, en este caso contamos con 3 procesos de captura de datos distintos, por tanto requerimos 3 tópicos. **Nota:** Este es el nombre con el que aparecerá en el servidor de la nube, por tanto debe ser un nombre descriptivo sobre el dato que se desea capturar.
- De las líneas **237-254** se realiza la captura de los datos que provienen de los otros procesos, estos datos son almacenados dentro de los campos de la estructura **"signal_tag_tx"** de forma ordenada correspondiente al nombre que se dio previo en el paso anterior.
- La invocación a la máquina de estados realizada en la línea **237** llama al proceso de publicación una vez la conexión con el servidor ha sido exitosa.
- La función **Publish** ubicada en el archivo **add-on.c** línea **167**, es la encargada de formatear el **JSON** y recorer la estructura que contiene los valores capturados por cada uno de los procesos.
**Nota Importante:** Dado que el proceso de formateo es automático, en función de la cantidad de pares de variables almacenados en la estructura **"signal_tag_tx"**, se hace necesario el uso de punteros que permitan recorrer la estructura, este proceso es altamente dependiente del tipo de datos que se usó dentro de los campos de la estructura y el tamaño del buffer para la cantidad de caracteres del nombre de la variable, por tanto una modificación que implique cambiar el tipo de datos o el tamaño del buffer implica re calcular los offsets de estos punteros.

**Procedimiento cálculo de offsets:** Si es necesario re calcular los offsets, nos ubicamos en el archivo **add-on.c** línea **177** y declaramos la variable **tempi** según la necesitemos.
ie. si BUFFER_SIZE_TAGNAME = 20 y tempi se declara como un entero de 32 bit, la porción de código desde la **177** a **180** quedaría de la siguiente forma.
```c
uint32_t *tempi = &ev_signals;
tempi = tempi + 5;//Nota1
size_struct = sizeof(ev_signals);
elements_in_struct = size_struct / (BUFFER_SIZE_TAGNAME + sizeof(uint32_t));//Nota2
```
**Nota1:** El valor de 5 es debido a que desde la dirección base, necesitamos recorrer el tamaño equivalente a 5 enteros de 32bit para llegar al campo correspondiente al nombre, esto es debido a que el campo correspondiente al nombre requiere 20 caracteres de 8bit equivalentes a 160bits, si los dividimos en 32bit nos queda el valor de 5.

**Nota2:** Esta línea nos provee la cantidad de pares de datos actualmente en la estructura, por tanto dividimos la cantidad de bytes entregada por size_struct por el tamaño en bytes de un par de datos (Nombre de variable y valor), en este caso sería dividir por **24**, 20 del nombre mas 4 del entero de 32bits.

Posteriormente se debe realizar un procedimiento similar pero para los demás campos de la estructura, en este caso las lineas **197** y **198** lo hacen posible.

```c
 tempc = tempc + BUFFER_SIZE_TAGNAME + 4;//Nota1
 tempi = tempi + 6;//Nota2
```
**Nota1:** Dado que inicialemente **tempc** está declarado como **char** y además estaba ubicado en la dirección base se requiere recorrer el equivalente a 20 bytes correspondiente al nombre y 4 bytes al valor del entero correspondiente al valor de la variable para llegar hasta el siguiente campo de nombre dentro de la estructura.

**Nota2:** Como **tempi** es de tipo entero de 32bit se hace necesario recorrer 5 enteros de 32bit correspondiente a los 20 bytes del nombre, más 1 entero de 32 bit, para un total de 6.
