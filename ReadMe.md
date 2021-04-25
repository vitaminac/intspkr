# Manejador para un altavoz - Primera Parte del Proyecto de [UPM Sistemas empotrados y ubicuos](http://www.datsi.fi.upm.es/docencia/SEUM/#PROYECTO)

Desarrollamos un módulo que actúe de manejador de un dispositivo de caracteres que proporcione acceso a un altavoz conectado al equipo para

* PC normal con versión de Linux al menos 5.0
* Raaspberry Pi

Ambas plataformas incluyen hardware que implementa la funcionalidad de tipo [PWM](https://en.wikipedia.org/wiki/PC_speaker#Pulse-width_modulation) requerida para asegurar la correcta temporización de los sonidos sin necesidad de que el procesador tenga que intervenir en cada paso de la generación de los mismos. Usamos directivas condicionales del preprocesador para aquellas partes que sean específicas de cada plataforma.

## Descripción general de las tareas a realizar

- [x] El dispositivo se denominará /dev/intspkr. Ese fichero especial se deberá crear automáticamente al cargarse el módulo.
- [x] El dispositivo podrá usar cualquier major, pero el minor lo recibirá como parámetro (minor), teniendo un valor por defecto de 0.
- [ ] Para enviar datos al altavoz, el proceso usará la llamada write. Evidentemente, por la propia esencia del dispositivo, no habrá una llamada read, aunque sí se implementará una función ioctl que ofrecerá diversos mandatos (como, por ejemplo, enmudecer el altavoz).
- [x] Cada 4 bytes que se escriben en el dispositivo definen un sonido: los dos primeros bytes especifican la frecuencia, mientras que los dos siguientes determinan la duración de ese sonido en milisegundos. 
- [ ] Completada la duración, el altavoz se desactivará, a no ser que haya más sonidos pendientes de generarse, ya sean parte del mismo write o de dos sucesivos.
- [ ] Un sonido con un valor de frecuencia de 0 corresponderá a desactivar el altavoz durante la duración indicada (se trata de un silencio o pausa en la secuencia de sonidos). 
- [ ] Dado que un sonido con una duración de 0 no tendrá ningún efecto, se podrá implementar como se considere oportuno siempre que tenga ese comportamiento nulo. 
- [x] Si el manejador recibe menos de 4 bytes, al no tratarse de un sonido completo, guardará esa información parcial hasta que una escritura posterior la complete. 
- [x] Una única operación de escritura puede incluir numerosos sonidos (así, por ejemplo, un write de 4KiB incluiría 1024 sonidos), que se ejecutarán como una secuencia.
- [ ] Por la propia idiosincrasia del dispositivo, se ofrecerá acceso exclusivo al mismo: solo se permitirá que lo tenga abierto un único proceso en modo escritura, retornando el error -EBUSY si un segundo proceso intenta abrirlo también en ese modo. Nótese que, sin embargo, sí se permite que varios procesos puedan abrir el dispositivo en modo lectura, no para leerlo, que no es posible, sino para poder solicitar operaciones ioctl sobre el mismo.
- [ ] Dado que se pretende que el código desarrollado sea válido para sistemas multiprocesador con un núcleo expulsivo, se debe realizar una correcta sincronización tanto entre llamadas concurrentes como entre una llamada y la rutina asíncrona de tratamiento del temporizador requerido para implementar la duración de cada sonido. Nótese que, aunque el dispositivo es de acceso exclusivo, distintos threads de un proceso (o un proceso y sus descendientes) pueden realizar llamadas write concurrentes al manejador usando el mismo descriptor de fichero, que habrá que secuenciar.
- [ ] Para optimizar el funcionamiento del dispositivo, maximizar el paralelismo entre la aplicación y la operación del dispositivo, y permitir un mejor secuenciamiento de los sonidos generados por la aplicación (lo que podría considerarse una especie de streaming), el manejador se implementará con un esquema de escritura diferida basado en un modelo productor-consumidor, utilizando un buffer interno para desacoplar al productor (la aplicación) y el consumidor (el propio dispositivo). De esta forma, la operación de escritura copiará los datos al buffer retornando inmediatemente, excepto si no hay espacio en el mismo, circunstancia en la que se bloqueará. En el caso de que el tamaño de los datos a escribir sea mayor que el de la cola, deberá realizar varias iteraciones de copia en modo sistema, con sus correspondientes bloqueos, hasta completar la llamada. Por otro lado, cada vez que se active la rutina de temporización que indica el final de un sonido, esta comprobará si hay más sonidos almacenados en la cola y, en caso afirmativo, iniciará la reproducción del siguiente a la frecuencia especificada activando, asimismo, un temporizador para controlar la duración del mismo.
- [ ] El tamaño del buffer interno se recibirá como parámetro (buffer_size, que será igual al tamaño de una página, PAGE_SIZE, si no se especifica) y, por razones que se explican más adelante, su tamaño debería ser una potencia de 2, siendo redondeado por exceso para cumplir ese requisito en caso de no serlo.
- [ ] Para reducir el número de cambios de contexto pero maximizando el paralelismo entre la aplicación y la reproducción de sonidos, si hay un proceso bloqueado debido a que el buffer está lleno, no se le desbloqueará en cuanto se complete el procesado de un sonido sino que se esperará hasta que haya un hueco suficientemente grande en el buffer que, o bien, le permita completar su petición y volver a modo usuario, o bien sea mayor o igual que un cierto umbral mínimo recibido como parámetro (buffer_threshold, que será igual a buffer_size, si no se especifica, y, en cualquier caso, nunca será mayor que el tamaño del buffer).
- [ ] Con este modo de operación desacoplado, después de que la aplicación cierre el fichero (e incluso después de que desaparezca), todavía pueden quedar datos pendientes de procesar en el buffer interno, realizándose el procesado de los mismos de manera convencional. Sin embargo, este modo desacoplado presenta un problema a la hora de eliminar el módulo. Dado que la aplicación puede cerrar el fichero asociado al dispositivo aunque este siga en funcionamiento, si se intenta eliminar el módulo en esas circunstancias, el sistema operativo no pondrá ningún impedimento a esa operación de eliminación del módulo puesto que no hay un fichero abierto asociado al módulo, lo que causaría una situación catastrófica y un muy probable pánico en el sistema. El módulo debería asegurarse de que la operación de finalización del mismo realiza una desactivación ordenada del dispositivo, deteniendo el temporizador que pudiera estar activo y silenciando el altavoz.
- [ ] Además de las operaciones write e ioctl, se ofrece también la llamada fsync, que bloqueará al proceso hasta que se vacíe el buffer. Esta llamada permite a una aplicación tener una mayor sincronía en su interacción con el dispositivo si así lo prefiere. Así, por ejemplo, una aplicación podría invocar esta llamada antes del close para asegurarse de que se han procesado todos los sonidos que ha generado antes de terminar. Otro ejemplo con todavía mayor sincronía sería una aplicación que llamada a fsync después de cada write.
- [ ] Se proporcionará una operación ioctl para enmudecer (mute) o desenmudecer el altavoz, según el valor del parámetro que reciba. Mientras está el altavoz enmudecido, se seguirán procesando los sonidos de la forma habitual, de manera que cuando se desenmudezca, se escuchará justo el sonido que esté reproduciéndose en ese instante (no se oirá nada en el caso de que no se esté procesando ningún sonido en ese momento o se esté trabajando con un silencio).
- [ ] Se proveerá de otra operación ioctl para hacer un reinicio del dispositivo vaciando la cola de sonidos pendientes de procesar. Sin embargo, se dejará que concluya el procesado del sonido actual, en caso de que lo hubiera. Los sonidos que se escriban después de esta operación se procesarán de la forma convencional.

## Antes de empezar: Entorno de desarrollo

En los últimos años algunos fabricantes han optado por prescindir altavoz interno. Afortunadamente, la plataforma VMware ofrece este dispositivo a sus máquinas virtuales. Instalar [Debian](https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-10.7.0-amd64-netinst.iso) en VMWare y configurar shared folder. 

### Configurar VMWare

Para poder compilarse con **make** hay que tener linux kernel header

    sudo apt install linux-headers-$(uname -r)

#### Configurar VMWare Shared Folders

    /usr/bin/vmhgfs-fuse .host:/ /home/shuxig/shares -o subtype=vmhgfs-fuse,allow_other

### Comprobacion de funcionamiento de pcspkr

Despues de la instalacion en VMware comprobar la funcionalidad de PC speaker virtual, es probable que tiene que reiniciar el sistema.

    sudo apt install beep
    sudo modprobe pcspkr
    beep -r 2

## Primera fase: Gestión del hardware del dispositivo

En esta primera fase se desarrollará todo el código de acceso al hardware requerido por el proyecto. En ella se implementará la funcionalidad específica de la plataforma para fijar la frecuencia del altavoz, así como su activación y desactivación (spkr_set_frequency, spkr_on y spkr_off). Asimismo, si es necesario incluir algún código específico de la plataforma para la configuración inicial del dispositivo o cuando finaliza el uso del mismo, se incorporará dentro de las funciones spkr_init y spkr_exit, respectivamente.

### Conocimiento previo de la primera fase

#### Acceso PIO a los registros del dispositivo

Linux proporciona un API interno para acceder a los puertos de entrada/salida de un dispositivo. Hay diversas funciones dependiendo del tamaño de la información transferida (8, 16 o 32) y si se trata de una lectura (in) o una escritura (out): inb, inw, inl, outb, outw y outl, respectivamente.

Además, existen versiones de esas mismas funciones con un sufijo _p (por ejemplo, inb_p), que realizan una muy breve pausa después de la operación de entrada/salida, que puede ser necesario en algunas arquitecturas.

#### Acceso MMIO a los registros del dispositivo

Para poder acceder a los registros del dispositivo cuando estos están conectados mediante MMIO a partir de una cierta dirección de memoria física, se requiere asociar un rango de direcciones lógicas a las direcciones físicas correspondientes a esos registros. La función ioremap realiza esta función recibiendo como parámetros el rango de direcciones físicas que se pretende acceder y devolviendo la dirección lógica a partir de la cual se puede acceder a ese rango:

    void *ioremap(unsigned long phys_addr, unsigned long size);

Aunque la dirección devuelta puede usarse como un puntero convencional en la mayoría de las plataformas, podría haber algunas en las que no es posible. Por ello, la documentación de Linux propone como una buena práctica usar las siguientes funciones para leer y escribir con ese puntero datos de diversos tamaños:

    unsigned int ioread8(void *addr);
    unsigned int ioread16(void *addr);
    unsigned int ioread32(void *addr);
    void iowrite8(u8 value, void *addr);
    void iowrite16(u16 value, void *addr);
    void iowrite32(u32 value, void *addr);

Una posible ventaja adicional de usar estas funciones en lugar del acceso directo mediante el puntero es que facilita la identificación de qué partes del código manipulan dispositivos de E/S. Asimismo, su uso en una plataforma basada en un procesador ARM, como el caso de una Raspberry Pi, incluye de forma transparente el uso de barreras requerido en este tipo de sistemas.

#### La sincronización en Linux

Se puede consultar el [capítulo 5](http://www.makelinux.net/ldd3/chp-5.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

Un aspecto fundamental y muy complejo en el desarrollo de un manejador es el control de todos los problemas de concurrencia que pueden presentarse durante la ejecución del mismo, y más si este está destinado a ejecutar en un sistema multiprocesador con un sistema operativo expulsivo, como ocurre con todo el código de Linux. De hecho, el grado de complejidad es tal que es prácticamente imposible verificar de una manera formal la total corrección de un determinado módulo y que el mismo está libre de condiciones de carrera.

Se pueden distinguir dos tipos de escenarios potencialmente conflictivos en lo que se refiere a la concurrencia y dos clases de soluciones. En cuanto a los escenarios conflictivos, se podrían clasificar como:

* Problemas de concurrencia entre llamadas concurrentes, es decir, entre eventos síncronos.
* Problemas de concurrencia entre una llamada y una interrupción, ya sea hardware o software, es decir, entre un evento síncrono y uno asíncrono.

Con respecto a las posibles soluciones, se podría distinguir entre:

##### Soluciones Bloqueantes

Este tipo de soluciones solo pueden usarse para resolver problemas entre llamadas concurrentes.

Como mecanismo bloqueante (es decir, adecuado para resolver problemas de sincronización entre llamadas), se puede usar el mutex, cuyo modo de operación es el habitual de esta clase de mecanismo.

* El tipo de datos que corresponde a un mutex es: **struct mutex**.
* La función para iniciarlo es: **mutex_init(struct mutex *m)**.
* Las funciones para obtenerlo son: **int mutex_lock(struct mutex *m)** e **int mutex_lock_interruptible(struct mutex *m)**. El sufijo interruptible indica que el proceso esperando por ese mutex puede salir del bloqueo por una señal, y suele ser la opción usada en la mayoría de las ocasiones. Cuando el proceso recibe una señal, **mutex_lock_interruptible** devuelve un valor negativo y, normalmente, la llamada al sistema debería terminar en ese momento devolviendo el error **-ERESTARTSYS**.
* La función para liberarlo es: **int mutex_unlock(struct mutex *m)**.

##### Soluciones basadas en espera activa

Es la única solución posible para resolver los problemas de concurrencia entre una llamada y una rutina de interrupción. Esta solución también se puede aplicar al problema de concurrencia entre llamadas. Esta clase de soluciones solo se puede aplicar a situaciones donde la sección crítica es corta y no incluye bloqueos.

En cuanto al mecanismo de sincronización basado en espera activa (es decir, adecuado para resolver problemas de sincronización entre una llamada y una interrupción), se van a presentar los spinlocks convencionales (Linux ofrece una variedad de mecanismos de este tipo: spinlocks de lectura/escritura, seqlocks, RCU locks...).

* El tipo de datos asociado a un spinlock es: **spinlock_t**.
* La función de iniciación es: **spin_lock_init(spinlock_t *lock)**.
* Las funciones para adquirir un spinlock son: 
  * **void spin_lock(spinlock_t *lock)**, que simplemente solicita el spinlock
  * **[void spin_lock_bh(spinlock_t *lock)](https://www.kernel.org/doc/htmldocs/kernel-locking/lock-user-bh.html)**, que inhibe las interrupciones software.
  * **void spin_lock_irq(spinlock_t *lock)**, que, además, inhibe las interrupciones hardware
  * **void spin_lock_irqsave(spinlock_t *lock, unsigned long flags)**, que, además guardando el estado de las mismas para poder restaurarlo posteriormente.
* Las correspondientes funciones de liberación del spinlock son: **void spin_unlock(spinlock_t *lock)**, **void spin_unlock_bh(spinlock_t *lock)**, **void spin_unlock_irq(spinlock_t *lock)**, **void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)**.
* La version ligera de **spinlock_t** es **raw_spinlock_t** que tiene menos  uso de la memoria y menos impacto de rendimiento.

##### Productor y Consumidor

La mejor forma de resolver problema de sincronizacion es evitarlo radicalmente escribiendo código que no requiera secciones críticas. Así, por ejemplo, el tipo de datos kfifo de Linux, que implementa una cola FIFO y que será recomendado para implementar el manejador planteado en este proyecto, está diseñado de manera que no es necesario ningún tipo de sincronización siempre que haya solo un productor y un consumidor.

##### Variable Atomica

Cuando una sección crítica solo implica la actualización de una variable escalar, en vez de usar alguno de los mecanismos presentados en esta sección, puede realizarse un acceso atómico a esa variable. La variable debe declararse como atomic_t y usarse las operaciones atómicas que proporciona Linux (atomic_set, atomic_xchg...).

#### Aspectos hardware específicos de la plataforma PC

##### Historia del altavoz interno

El altavoz interno ya estaba presente en el primer PC de IBM ([modelo 5150](https://web.archive.org/web/20120222235548/http://computermuseum.usask.ca/articles/IBM-5150-Specifications.pdf)) como único elemento capaz de producir sonidos en el equipo. El modo de operación no ha cambiado desde entonces: la salida del temporizador 2 del sistema está conectada al altavoz lo que permite enviar formas de onda al mismo que generan sonidos con una frecuencia que depende del contador que se haya cargado en ese temporizador. Nótese que con esta solución el software no está involucrado directamente en la generación del sonido (lo cual era un requisito dada la limitada potencia de los procesadores de entonces), restringiéndose su labor a cargar el temporizador con un valor que corresponda a la frecuencia deseada y, a continuación, activar el altavoz, desactivándolo cuando ya no se desee que continúe el sonido.

Este hardware de sonido tenía una calidad y una versatilidad muy limitadas. Aún así, con ingenio y aprovechando ciertas propiedades físicas de estos primeros altavoces, que eran de tipo dinámico (magnet-driven), se idearon técnicas de tipo PWM, que conseguían generar sonidos más complejos, aunque de calidad limitada, sobre este hardware, requiriendo, eso sí, la dedicación exclusiva del procesador para llevarlas a cabo. Estas técnicas dejaron de ser efectivas con la progresiva sustitución de los altavoces dinámicos por los piezoelécticos, que no tenían esas propiedades físicas.

La aparición de las tarjetas de sonido, con una calidad de audio muy superior, eclipsó a los altavoces internos que quedaron relegados básicamente a una importante misión: ser el medio de diagnosticar errores en el arranque del sistema cuando ni siquiera puede estar accesible la pantalla y menos todavía la tarjeta de sonido.

##### Modo de operación del altavoz interno

En esta sección se revisará brevemente el hardware de [PC speaker](https://en.wikipedia.org/wiki/PC_speaker). Sin embargo, de forma intencionada, no se entrará en detalle sobre el mismo porque se considera, desde un punto de vista didáctico, que esa labor de búsqueda de información sobre un componente hardware y el estudio detallado de su modo de operación es parte de los conocimientos prácticos que se pretende que alcance el alumno como parte de este proyecto. En cualquier caso se sugieren dos referencias de OSDev que presentan en detalle el hardware involucrado, así como aspectos sobre su programación ([PIT](https://wiki.osdev.org/Programmable_Interval_Timer) y [PC Speaker](https://wiki.osdev.org/PC_Speaker)).

Desde su diseño original, el PC dispone de un circuito denominado Programmable Internal Timers (PIT; originalmente, chip 8253/8254, aunque actualmente su funcionalidad está englobada en chips que integran muchas otras funciones adicionales) que ofrece tres temporizadores (denominados también canales): el primero (timer 0), conectado a la línea de interrupción IRQ0 y usado normalmente como temporizador del sistema operativo, el segundo (timer 1), asociado al refresco de la memoria dinámica, y el tercero (timer 2), conectado al altavoz interno. Estos temporizadores pueden actúar como divisores de frecuencia de una entrada de reloj que oscila, por razones históricas, a 1,193182 MHZ (constante PIT_TICK_RATE en el código de Linux). Los dos primeros temporizadores tienen directamente conectada esa señal de entrada, pero para el tercero, que es el que nos interesa, es necesario conectarla explícitamente, como explicaremos en breve.

Para configurar uno de estos temporizadores, en primer lugar, es necesario seleccionar su modo de operación (de un solo disparo, de onda cuadrada...) escribiendo en su registro de control (puerto 0x43) el valor correspondiente (en nuestro caso, hay que seleccionar el timer 2 y especificar un modo de operación 3, denominado square wave generator, que actúa como un divisor de frecuencia). A continuación, hay que escribir en su registro de datos (puerto 0x42) el valor del contador del temporizador (el divisor de frecuencia). Al tratarse de un puerto de 8 bits y dado que el contador puede tener hasta 16 bits, se pueden necesitar dos escrituras (primero la parte baja del contador y luego la alta) para configurar el contador.

La operación de configuración que acaba de describirse determinará la forma de onda que alimenta el altavoz (el código correspondiente deberá incluirse en la función spkr_set_frequency del fichero spkr-io.c). Sin embargo, no causa que el altavoz comience a sonar. Como se comentó previamente, para activar el altavoz (el código correspondiente deberá incluirse en la función spkr_on del fichero spkr-io.c), es necesario, en primer lugar, conectar el reloj del sistema a la entrada del temporizador 2, lo que requiere escribir un 1 en el bit 1 del puerto 0x61. Sin embargo, eso no es suficiente: la salida del temporizador 2 se mezcla mediante una función AND con el bit 0 del puerto 0x61, por lo que habrá que escribir un 1 en dicho bit para que la forma de onda llegue a la entrada del altavoz. Por tanto, recapitulando, hay que escribir un 1 en los dos bits de menor peso del puerto 0x61. Téngase en cuenta que este puerto, denominado puerto B de control del sistema, está involucrado en la configuración de otros componentes del sistema, por lo que, a la hora de escribirse los dos primeros bits a 1, hay que asegurarse de que los bits restantes permanecen inalterados.

Para desactivar el altavoz, es suficiente con escribir un 0 en cualquiera de esos bits (el código correspondiente deberá incluirse en la función spkr_off del fichero spkr-io.c).

##### Soporte del altavoz interno en Linux

Linux incluye dos manejadores para el altavoz interno: los módulos **pcspkr** y **snd_pcsp**. Ambos permiten enviar al altavoz formas de onda de la frecuencia especificada. El segundo, además, permite gestionar sonidos más complejos usando las técnicas de PWM comentadas previamente y solo tiene sentido usarlo en un sistema sin tarjeta de sonido. Evidentemente, cuando se quiera probar el código escrito, no debe estar cargado ninguno de estos dos módulos, sino el desarrollado como parte del proyecto.

En cuanto al API ofrecido a las aplicaciones, ofrece dos tipos de servicios:

* Basados en ioctls dirigidos a la consola (fichero /dev/console). Concretamente, proporciona dos operaciones (man console_ioctl): KDMKTONE, que genera un sonido del periodo (inverso de la frecuencia) y duración en milisegundos especificados por la parte baja y alta del parámetro de la operación (obsérvese las similitudes con el esquema propuesto en este proyecto), y KIOCSOUND, que produce un sonido del periodo especificado que se mantendrá hasta que se genere otro sonido o se realice esta misma operación especificando un valor 0 como parámetro.
* Basados en caracteres de escape dirigidos a la consola (fichero /dev/console). Cuando un proceso escribe el carácter ASCII 7 (Control-G, BELL o \a) en la consola, se produce un sonido cuya frecuencia y duración es configurable (man console_codes): la secuencia ESC[10;m] fija en m la frecuencia del sonido, mientras que la secuencia ESC[11;n] establece como n la duración en milisegundos.

Evidentemente, la breve explicación de estas APIs solo tiene carácter didáctico, puesto que no se van a usar en este proyecto, aunque también sirve como comparativa con el API que se propone en este trabajo. Así, por ejemplo, se puede observar que estas APIs, a diferencia de la propuesta en este proyecto, no permiten que la aplicación especifique una secuencia de sonidos con una sola operación.

#### Aspectos hardware específicos de la plataforma Raspberry Pi

TODO:

### Funcionalidad a desarrollar en la plataforma PC

* **spkr_set_frequency**: Se trata de manipular mediante PIO (funciones inb, outb, etc.) los puertos 0x42 y 0x43 para fijar la frecuencia.
  * Respecto al cálculo de la frecuencia requerida realizado dentro de la función spkr_set_frequency, hay que determinar cuál es el divisor de frecuencia correspondiente teniendo en cuenta la frecuencia del reloj de entrada al temporizador (constante PIT_TICK_RATE en el código de Linux) y la frecuencia deseada.
* **spkr_on**, **spkr_off**: Se trata de manipular mediante PIO (funciones inb, outb, etc.) el puerto 0x61 para activar y desactivar la llegada de la señal periódica al altavoz.
  * Recuerde que debe leerse previamente el registro 0x61 para mantener el resto de los valores que contiene.
* **sincronización**: Cuando se programa el chip 8253/8254 es necesario realizar dos escrituras al registro de datos para enviar primero la parte baja del contador y, a continuación, la parte alta. Por tanto, es necesario crear una sección crítica en el acceso a ese hardware. Hay diversas partes del código de Linux que acceden a ese hardware y usan un spinlock (denominado i8253_lock de tipo raw spinlock, que es más eficiente pero menos robusto que un spinlock convencional, y que se incluye en el fichero linux/i8253.h) para controlar el acceso.
  * Dado que este hardware se accede tanto desde el contexto de una llamada como desde el de una interrupción, hay que usar las funciones que protegen también contra las interrupciones: raw_spin_lock_irqsave y raw_spin_unlock_irqrestore.

### Funcionalidad a desarrollar en la plataforma Raspberry Pi

* **sprk_init**: habría que realizar en primer lugar las operaciones ioremap necesarias para tener acceso a las tres zonas de memoria asociadas a los tres tipos de dispositivos a configurar:
  * **Reloj PWM**: tal como se explicó previamente, hay que escribir en sus registros de configuración (CM_PWMCLKDIV y CM_PWMCLKCTL) los valores adecuados para definir el divisor de frecuencia y activar el reloj asociándolo al oscilador. Con respecto al cálculo del divisor de frecuencia, se recomienda un divisor de 16, que, dado que el oscilador genera una frecuencia de aproximadamente 19,2MHZ, proporciona una frecuencia resultante de 1,2MHz, que es similar a la usada en la plataforma PC.
  * **Configuración de GPIO18 como PWM0**: como se detalló anteriormente, esta operación requiere escribir en la posición correspondiente del registro GPFSEL1 los tres bits requeridos, pero leyendo previamente el contenido del registro para respetarlo.
  * **Configuración del canal 1 de PWM en el modo M/S**: tal como se describió antes, hay que modificar el registro CTRL para especificar este modo pero sin activar todavía el canal, respetando el contenido previo. Realmente, no sería estrictamente necesario realizar esta operación en este momento pudiéndose incluir en la propia acrivación del canal que se lleva a cabo en spkr_on.
* **spkr_set_frequency**: en esta función se escribirá en los registros RNG1 y DAT1 los valores requeridos por la frecuencia solicitada. Con respecto al primer valor, observe que actúa como un divisor de frecuencia, por lo que basta con comparar la frecuencia configurada en el reloj PWM (1,2MHz) y la recibida como parámetro. En cuanto al segundo valor, al requerirse una onda cuadrada, será la mitad que el primero (lo que resulta en un duty cycle del 50%).
* **spkr_on**: se modificará el registro CTRL, respetando su contenido previo, para activar el canal 1. Nótese que en caso de que no se hubiera especificado el modo M/S en la iniciación, habría que hacerlo en este momento.
* **spkr_off**: se modificará el registro CTRL, respetando su contenido previo, para desactivar el canal 1.
* **spkr_exit**: se realizarán las operaciones iounmap requeridas.

### Pruebas de primera fase

Para probar el software desarrollado en esta fase, dado que todavía no se ha creado el dispositivo como tal, se propone incluir en la función de inicio del módulo una llamada para fijar la frecuencia del altavoz, según el valor recibido en un parámetro entero denominado frecuencia, y, a continuación, una segunda llamada para activarlo. Asimismo, en la llamada de fin del módulo, se realizaría la invocación de la función que desactiva el altavoz. De esta manera, al cargar el módulo, se debería escuchar un sonido de la frecuencia correspondiente al parámetro recibido y al descargarlo debería detenerse la reproducción del mismo.

#### Comprobar la informacion de modulo

    sudo modinfo ./intspkr.ko

#### Para cargar el modulo

    sudo insmod ./intspkr.ko

#### Para descargar el modulo

    sudo rmmod intspkr

#### Para debug

    sudo dmesg

## Segunda fase: Alta y baja del dispositivo

### Conocimiento previo de segunda fase

#### Reserva y liberación de números major y minor

Se puede consultar la [sección 2 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-2.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

Un dispositivo en Linux queda identificado por una pareja de números: el major, que identifica al manejador, y el minor, que identifica al dispositivo concreto entre los que gestiona ese manejador. El tipo **dev_t** mantiene un identificador de dispositivo dentro del núcleo. Internamente, como se comentó previamente, está compuesto por los valores major y minor asociados, pudiéndose extraer los mismos del tipo identificador:

    dev_t myDevice; 
	  .....
	  int major, minor;
	  major = MAJOR(myDevice);
	  minor = MINOR(myDevice);

O viceversa:

    int major, minor;
  	.....
  	dev_t myDevice; 
  	myDevice = MKDEV(major, minor);

Antes de dar de alta un dispositivo de caracteres, hay que reservar sus números major y minor asociados. En caso de que se pretenda que el número major lo elija el propio sistema, como ocurre en este caso, se puede usar la función alloc_chrdev_region (definida en **#include <linux/fs.h>**) , que devuelve un número negativo en caso de error y que tiene los siguientes parámetros:

    int alloc_chrdev_region(dev_t *dev, unsigned int firstminor, unsigned int count, char *name);

1. Parámetro solo de salida donde nos devuelve el tipo dev_t del primer identificador de dispositivo reservado.
2. Parámetro de entrada que representa el minor del identificador de dispositivo que queremos reservar (el primero de ellos si se pretenden reservar varios).
3. Parámetro de entrada que indica cuántos números minor se quieren reservar.
4. Parámetro de entrada de tipo cadena de caracteres con el nombre del dispositivo.

En caso de que se quiera usar un determinado número major se utiliza en su lugar la función register_chrdev_region. Linux tiene [una list de numeros de major reservado](https://www.kernel.org/doc/Documentation/admin-guide/devices.txt) definido en [major.h](https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/major.h#L11).

La operación complementaria a la reserva es la liberación de los números major y minor asociados. Tanto si la reserva se ha hecho con **alloc_chrdev_region** como si ha sido con **register_chrdev_region**, la liberación se realiza con la función **unregister_chrdev_region**, que recibe el primer identificador de dispositivo a liberar, de tipo dev_t, así como cuántos se pretenden liberar.

    void unregister_chrdev_region(dev_t first, unsigned int count);

#### Alta y baja de un dispositivo dentro del núcleo

Se puede consultar la [sección 4 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-4.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

Por ahora, solo hemos reservado un número de identificador de dispositivo formado por la pareja major y minor. A continuación, es necesario "crear un dispositivo" asociado a ese identificador, es decir, dar de alta dentro del núcleo la estructura de datos interna que representa un dispositivo de caracteres y, dentro de esta estructura, especificar la parte más importante: el conjunto de funciones de acceso (apertura, cierre, lectura, escritura...) que proporciona el dispositivo.

El tipo que representa un dispositivo de caracteres dentro de Linux es struct cdev (no confundir con el tipo dev_t, comentado previamente, que guarda un identificador de dispositivo; nótese, sin embargo, que, como es lógico, dentro del tipo struct cdev hay un campo denominado dev de tipo dev_t que almacena el identificador de ese dispositivo), que está definido en **#include <linux/cdev.h>**.

Se debe definir una variable de este tipo (o una estructura que la contenga) y usar la función cdev_init para dar valor inicial a sus campos. El primer parámetro será la dirección de esa variable que se pretende iniciar y el segundo una estructura de tipo **struct file_operations** que especifica las funciones de servicio del dispositivo (véase [sección 3 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-3.shtml) del libro LDD):

    void cdev_init(struct cdev *cdev, struct file_operations *fops);

A continuación, se muestra un ejemplo de uso de la estructura file_operations que especifica solamente las operaciones de apertura, cierre y escritura.

    static int ejemplo_open(struct inode *inode, struct file *filp) {
      .....
    }

    static int ejemplo_release(struct inode *inode, struct file *filp) {
      .....
    }

    static ssize_t ejemplo_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
      .....
    }

    static struct file_operations ejemplo_fops = {
            .owner =    THIS_MODULE,
            .open =     ejemplo_open,
            .release =  ejemplo_release,
            .write =    ejemplo_write
    };

Después de iniciar la estructura que representa al dispositivo, hay que asociarla con los identificadores de dispositivo reservados previamente. Para ello, se usa la función **cdev_add**:

    int cdev_add(struct cdev *dev, dev_t num, unsigned int count);

siendo el tercer parámetro igual a 1 en nuestro caso, puesto que queremos dar de alta un único dispositivo.

Finalmente, la operación de baja del dispositivo se lleva a cabo mediante la función **cdev_del**:

    void cdev_del(struct cdev *dev);

Nótese que esta operación siempre hay que hacerla antes de liberar el identificador de dispositivo (**unregister_chrdev_region**).

#### Alta de un dispositivo para su uso desde las aplicaciones

Aunque después de dar de alta un dispositivo dentro del núcleo ya está disponible para dar servicio a través de sus funciones de acceso exportadas, para que las aplicaciones de usuario puedan utilizarlo, es necesario crear un fichero especial de tipo dispositivo de caracteres dentro del sistema de ficheros.

Anteriormente a la incorporación del modelo general de dispositivo en Linux y el **sysfs**, era necesario que el administrador invocara los mandatos **mknod** necesarios para crear los ficheros especiales requeridos. Con la inclusión de este modelo, el manejador solo debe dar de alta el dispositivo en el **sysfs**, encargándose el proceso demonio de usuario **udevd** de la creación automática del fichero especial (en este caso, **/dev/intspkr**).

El primer paso que se debe llevar a cabo es la creación de una clase en **sysfs** para el dispositivo gestionado por el manejador usando para ello la llamada **class_create** (todo dispositivo tiene que pertenecer a una clase) que retorna el identificador de la clase creada (**NULL** en caso de error):

    #include <linux/device.h>
    ...
    struct class * class_create (struct module *owner, const char *name);

Como primer parámetro se especificaría **THIS_MODULE** y en el segundo el nombre que se le quiere dar a esta clase de dispositivos (en este caso, **intspkr**). Después de ejecutar esta llamada, aparecerá la entrada correspondiente en sysfs (**/sys/class/intspkr/**).

A la hora de descargar el módulo habrá que hacer la operación complementaria (**class_destroy(struct class * clase)**).

Después de crear la clase, hay que dar de alta el dispositivo de esa clase. Para ello, se usa la función **device_create**:

    struct device * device_create(struct class *class, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...)

Para el ejemplo que nos ocupa, solo son relevantes los siguientes parámetros (para los demás se especificará un valor NULL):

* El primer parámetro corresponde al valor retornado por la llamada que creó previamente la clase.
* El tercero corresponde al dispositivo creado previamente.
* El quinto al nombre que se le quiere dar a la entrada en **sysfs**.

En nuestro caso, habrá que hacer una llamada a esta función especificando como nombre de dispositivo **intspkr**, creándose la entrada **/sys/class/intspkr/intspkr**. Nótese que el demonio de usuario **udevd** creará automáticamente en **/dev** la entrada correspondiente con los números major y minor seleccionados:

A partir de este momento, las llamadas de apertura, lectura, escritura, cierre, etc. sobre esos ficheros especiales son redirigidas por el sistema operativo a las funciones de acceso correspondientes exportadas por el manejador del dispositivo.

La llamada **device_destroy(struct class * class, dev_t devt)** da de baja el dispositivo en **sysfs**.

### Funcionalidad a desarrollar

En primer lugar, hay que reservar para el dispositivo un número major, elegido por el propio núcleo, y un número minor, que corresponde al especificado como parámetro del módulo con el nombre minor (0 si no se recibe ningún parámetro).

Para ello, se debe incluir dentro de la rutina de iniciación del módulo una llamada a **alloc_chrdev_region** para reservar un dispositivo llamado **intspkr**, cuyo major lo seleccionará el sistema, mientras que el minor corresponderá al valor recibido como parámetro. Asimismo, añada a la rutina de terminación del módulo la llamada a la función **unregister_chrdev_region** para realizar la liberación correspondiente.

Una vez reservado el número de dispositivo, hay que incluir en la función de carga del módulo la iniciación (**cdev_init**) y alta (**cdev_add**) del dispositivo. Asimismo, se debe añadir el código de eliminación del dispositivo (**cdev_del**) en la rutina de descarga del módulo.

Para dar de alta al dispositivo en **sysfs**, en la iniciación del módulo se usarán las funciones **class_create** y **device_create**. En la rutina de descarga del módulo habrá que invocar a **device_destroy** y a **class_destroy** para dar de baja el dispositivo y la clase en **sysfs**.

En cuanto a las funciones exportadas por el módulo, en esta fase, solo se especificarán las tres operaciones presentadas previamente (apertura, escritura y liberación). Además, dichas funciones solo imprimirán un mensaje con printk mostrando qué función se está invocando. Estos mensajes se deben mantener en la versión final de la práctica.

Para evitar que los programas que usen este dispositivo todavía incompleto se comporten de manera errónea, las funciones de apertura y liberación deberían devolver un 0, para señalar que no ha habido error, y la función de escritura debería devolver el mismo valor que recibe como tercer parámetro (count), para indicar que se han procesado los datos a escribir.

### Pruebas de segunda fase

Para comprobar el funcionamiento correcto del módulo, puede añadir un **printk** que muestre qué major se le ha asignado al dispositivo y usar el mandato **dmesg**, que muestra el log del núcleo, para poder verificarlo (antes de hacer una prueba, se recomienda vaciar el log, usando **dmesg --clear**, para facilitar las verificaciones sobre la información impresa). 

#### Comprobar entrada de dispositivo configurado

Asimismo, después de la carga pero antes de la descarga, puede comprobar que se ha creado una entrada correspondiente al nuevo dispositivo, y que esta desaparece cuando el módulo es descargado. 

    cat /proc/devices | grep intspkr

#### Comprobar entrada de sysfs

Asimismo, una vez cargado el módulo, debe comprobar que se han creado los ficheros correspondientes en **sysfs**, así como el fichero especial asociado con el dispositivo.

    ls -l /sys/class/intspkr /sys/class/intspkr/intspkr /dev/intspkr

#### Comprobar asignacion de minor

Pruebe a cargar el módulo, sin especificar el número **minor** como parámetro y haciéndolo, y compruebe que el fichero especial creado tiene el número de **minor** correctamente asignado.

    sudo insmod ./intspkr.ko minor=30
    ls -l /dev/intspkr

#### Comprobar funcion de servicio

A continuación, pruebe a ejecutar el siguiente mandato para comprobar que se activan las funciones de apertura, escritura y cierre:

    sudo chmod 666 /dev/intspkr
    echo X > /dev/intspkr

## Tercera fase: Operaciones de apertura y cierre

### Conocimiento previo de tercera fase

#### Parámetros del open

Se puede consultar la [sección 3 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-3.shtml) y la [sección 5 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-5.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

La llamada de apertura recibe dos parámetros:

* Un puntero al inodo de fichero (**struct inode *inode**). Desde el punto de vista del diseño de un manejador, el campo más interesante de esa estructura es **struct cdev *i_cdev** y es un puntero a la estructura cdev que se usó al crear el dispositivo.
* Un puntero a la estructura file (**struct file *file**) que representa un descriptor de un fichero abierto. De cara el proyecto, el único campo relevante es **f_mode**, que almacena el modo de apertura del fichero. Para determinar el modo de apertura, se puede comparar ese campo con las constantes **FMODE_READ** y **FMODE_WRITE**.

### Funcionalidad a desarrollar de la tercera fase

Hay que asegurarse de que en cada momento solo está abierto una vez en modo escritura el fichero. Si se produce una solicitud de apertura en modo escritura estando ya abierto en ese mismo modo, se retornará el error **-EBUSY**. Nótese que, sin embargo, no habrá ninguna limitación con las aperturas en modo lectura.
Téngase en cuenta que pueden ejecutarse concurrentemente varias activaciones de la operación de apertura (ya sea porque se ejecutan en distinto procesador o, aunque se ejecuten en el mismo, lo hacen de forma intercalada al tratarse de un núcleo expulsivo). Por tanto, se deberá asegurar de que no se producen condiciones de carrera.

### Pruebas de la tercera fase

A continuación se propone una prueba para verificar si la funcionalidad se ha desarrollado correctamente:

Se arranca en una ventana un escritor y se le deja parado sin escribir nada por la pantalla en background:

    cat > /dev/intspkr &

En una segunda ventana se ejecuta otro escritor y debe producirse un error(Dispositivo o recurso ocupado):

    cat > /dev/intspkr

Se lanza un proceso que abre el fichero en modo lectura para comprobar que no da error:

    cat /dev/intspkr

En la ventana inicial introducimos un Control-D para finalizar el proceso escritor y volvemos a lanzarlo para probar que se ha rehabilitado la posibilidad de abrir el fichero en modo escritura.

    jobs
    fg [tarea id]
    {introducimos control-D}
    cat > /dev/intspkr

## Cuarta fase: Operación de escritura

Esta es la fase central del proyecto en la que se desarrolla la funcionalidad principal del manejador. En ella, es necesario gestionar un buffer interno donde se irán almacenando los sonidos generados por la aplicación en espera de que se vayan reproduciendo. Para la implementación de ese buffer que tiene un comportamiento de cola **FIFO**, se presentan dos opciones: programar una estructura de datos que implemente esa cola o, la opción recomendada, usar un tipo de datos interno de Linux, denominado **kfifo**, que implementa esa funcionalidad.

Si se opta por la primera alternativa, es necesario conocer cómo se reserva memoria dinámica en Linux, mientras que si se selecciona la segunda, no es estrictamente necesario conocer este aspecto puesto que esa reserva de memoria se realiza internamente al iniciar una instancia del tipo **kfifo**. En cualquier caso, a continuación, se va a incluir información sobre cómo se solicita y libera memoria dinámica dentro del núcleo de Linux.

### Conocimiento previo de la cuarta fase

#### Reserva y liberación de memoria dinámica

Puede consultar la [sección 1 del capítulo 8](http://www.makelinux.net/ldd3/chp-8-sect-1.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

Las funciones para reservar y liberar memoria dinámica son kmalloc y kfree, cuyas declaraciones son las siguientes:

    #include <linux/slab.h>
    void *kmalloc(size_t size, int flags);
    void kfree(const void *);

Sus prototipos son similares a las funciones correspondientes de la biblioteca de C. La única diferencia está en el parámetro flags, que controla el comportamiento de la reserva. Los tres valores más usados para este parámetro son:

* **GFP_KERNEL**: Reserva espacio para uso del sistema operativo pero asumiendo que esta llamada puede bloquear al proceso invocante si es necesario. Es el que se usa para reservar memoria en el contexto de una llamada al sistema.
* **GFP_ATOMIC**: Reserva espacio para uso del sistema operativo pero asegurando que esta llamada nunca se bloquea. Es el método que se usa para reservar memoria en el ámbito de una rutina de interrupción.
* **GFP_USER**: Reserva espacio para páginas de usuario. Se usa en la rutina de fallo de página para asignar un marco al proceso.

En caso de error, la llamada kmalloc devuelve NULL. Habitualmente, en ese caso se termina la función correspondiente retornando el error **-ENOMEM**.

#### Acceso al mapa de usuario del proceso

Se puede consultar [la sección 7 del capítulo 3](http://www.makelinux.net/ldd3/chp-3-sect-7.shtml#chp-3-ITERM-4602) y [la sección 1 del capítulo 6](http://www.makelinux.net/ldd3/chp-6-sect-1.shtml#chp-6-ITERM-5180) del libro LDD para profundizar en los aspectos presentados en este apartado.

Habitualmente, un manejador necesita acceder al mapa de memoria de usuario para leer información del mismo, en el caso de una escritura, y para escribir información en él, si se trata de una lectura. Para ello, se proporcionan funciones similares a la rutina estándar de C memcpy que permiten copiar datos desde el espacio de usuario al de sistema (**copy_from_user**) y viceversa (**copy_to_user**). Asimismo, se ofrecen macros que permite copiar un único dato en ambos sentidos (**get_user** y **put_user**, respectivamente).

    #include <asm/uaccess.h>
    unsigned long copy_from_user(void __user *to, const void *from, unsigned long n);
    unsigned long copy_to_user(void *to, const void __user *from, unsigned long n);
    int put_user(datum,ptr);
    int get_user(local,ptr);

En caso de éxito, estas funciones devuelven un 0. Si hay un fallo, que se deberá a que en el rango de direcciones del **buffer** de usuario hay una o más direcciones inválidas, devuelve un valor distinto de 0 que representa el número de bytes que no se han podido copiar.

Normalmente, si se produce un error, la función correspondiente devuelve el error **-EFAULT**.

#### Bloqueo y desbloqueo de procesos

A continuación, se explican algunos conceptos básicos de la gestión de bloqueos y desbloqueos de procesos en Linux.

##### Cola de espera

La gestión de bloqueos se basa en el mecanismo de colas de espera (**wait queues**). El siguiente ejemplo muestra cómo declarar e iniciar una cola de espera que, como es frecuente, está incluida dentro de la estructura que almacena la información del dispositivo:

    #include <linux/wait.h>
    #include <linux/sched.h>
    
    // declaración de una variable dentro de una estructura
    struct info_dispo {
        wait_queue_head_t lista_bloq;
        ................
    } info;
    // iniciación de la variable (se suele hacer en la carga del módulo)
    init_waitqueue_head(&info.lista_bloq);

Para el bloqueo del proceso, Linux proporciona varias funciones. En la práctica se propone usar la macro **wait_event_interruptible** cuyo prototipo es el siguiente:

    int wait_event_interruptible(wait_queue_head_t cola, int condicion);

Esta función causa que el proceso se bloquee en la cola hasta que sea desbloqueado y se cumpla la condición. Nótese que si en el momento de invocar la función ya se cumple la condición, no se bloqueará. Tenga en cuenta que la condición no solo se evaluará en el momento de invocación de la macro, sino todas las veces que sea necesario.

La función **wait_event_interruptible** devuelve un valor distinto de 0 si el bloqueo ha quedado cancelado debido a que el proceso en espera ha recibido una señal. Si es así, el tratamiento habitual es terminar la llamada devolviendo el valor **-ERESTARTSYS**, que indica al resto del sistema operativo que realice el tratamiento oportuno.

Para el desbloqueo de otro proceso, se propone usar la función wake_up_interruptible, que desbloquea a todos los procesos esperando en esa cola de procesos. Su prototipo es el siguiente:

    void wake_up_interruptible(wait_queue_head_t *cola);

Nótese que no es necesario comprobar que hay algún proceso bloqueado en la cola de espera (es decir, que la cola no está vacía) antes de usar esta función ya que esta comprobación la realiza la propia función.

##### KFIFO

Linux proporciona una implementación de una cola FIFO, que permite almacenar en la misma elementos de un determinado tamaño (por defecto, cada elemento es de tipo **unsigned char**, pero puede especificarse que sea de cualquier tipo). El diseño de la misma está cuidadosamente realizado de manera que no requiere ningún tipo de sincronización siempre que solo haya un flujo de ejecución productor y uno consumidor. Puede consultar el [API de kfifo](https://www.kernel.org/doc/htmldocs/kernel-api/kfifo.html).

A continuación, se comentan brevemente algunas de las funciones que se pueden requerir a la hora de llevar a cabo el proyecto:

* **kfifo_alloc**: crea una **cola FIFO** reservando en memoria dinámica el buffer requerido. El número de elementos que tendrá el buffer se especifica en el segundo parámetro y debe ser una potencia de 2 siendo redondeado por exceso para cumplirlo (ese requisito permite una gestión más eficiente del buffer). El tercer parámetro debe ser especificado conforme a lo explicado en la sección que describía cómo reservar memoria dinámica en Linux, ya que, al fin y al cabo, internamente esta función realiza un kmalloc.
* **kfifo_free**: libera el buffer y destruye la cola.
* **kfifo_is_full**, **kfifo_is_empty**, **kfifo_size**, **kfifo_len** y **kfifo_avail** retornan, respectivamente, si la cola está llena o vacía, el tamaño de la cola, el número de elementos almacenados en la misma y el número que todavía cabrían.
* **kfifo_out**: extrae datos de la cola FIFO. En el tercer parámetro se especifican cuántos elementos se quieren extraer y como valor de retorno devuelve cuántos realmente se han extraído.
* **kfifo_from_user**: copia datos del mapa de usuario a la cola FIFO. Esta macro usa internamente la función copy_from_user presentada en el apartado previo pudiendo, por tanto, devolver el mismo valor de error.
**kfifo_reset_out**: es equivalente a extraer todo el contenido actual del buffer pero sin copiarlo a ningún sitio.

##### Gestión de temporizadores

Puede consultar la [sección 4 del capítulo 7](http://www.makelinux.net/ldd3/chp-7-sect-4.shtml) del libro LDD para profundizar en los aspectos presentados en este apartado.

El API interno de Linux ofrece una colección de funciones que permiten gestionar temporizadores, es decir, permite solicitar que cuando transcurra una cierta cantidad de tiempo, se ejecute asíncronamente la función asociada al temporizador. A la hora de especificar el tiempo hay que hacerlo usando como unidad el periodo (inverso de la frecuencia) de la interrupción del reloj del sistema (es decir, el tiempo que transcurre entre dos interrupciones de reloj). El tiempo actual se almacena en la variable global jiffies y corresponde al número de interrupciones de reloj que se han producido desde que arrancó el sistema.

El API de gestión de temporizadores ha cambiado a partir de la versión del núcleo 4.15. Como se especificó al principio del documento, si está trabajando sobre una plataforma de tipo PC, debe usar una versión del núcleo al menos 5.0. En caso de que esté desarrollando el proyecto en una plataforma RaspBerry, se permite usar versiones de núcleos más antiguas, pudiendo tener que utilizar el API antiguo. Por no alargar más el documento, a continuación, se revisa brevemente el API actual. Por favor, si necesita información del API previa, busque la informacion correspondiente.

* **struct timer_list**: tipo de datos asociado a un temporizador. Aunque esta estructura tiene más campos, estamos interesados en los dos siguientes: **function**, que corresponde a la función que se invocará cuando se cumpla el temporizador, y **expires**, que determina cuándo se cumplirá el temporizador especificando qué valor deberá tener la variable **jiffies** para que se produzca su activación (así, por ejemplo, para especificar que el temporizador se debe activar cuando transcurra, al menos, 1 milisegundo, hay que especificar en este campo la suma del valor actual de la variable global **jiffies** más el número de interrupciones de reloj que corresponden a 1 milisegundo). Las funciones **jiffies_to_msecs** y **msecs_to_jiffies** calculan a cuántos milisegundos corresponde un determinado número de **jiffies** (es decir, de interrupciones de reloj) y viceversa.
* Para la iniciación del temporizador se usa la función **timer_setup**, que inicia la estructura asociada al temporizador, especificando tres parámetros:
  * El temporizador **(struct timer_list *)** que se pretende iniciar.
  * La función que se activará cuando se cumpla el temporizador **(void (*func)(struct timer_list *))**. Nótese que, por tanto, suponiendo que la función se denomina **f**, cuando venza el temporizador esa función recibirá como parámetro la dirección de la estructura **struct timer_list** correspondiente al temporizador que causó su activación **void f(struct timer_list *t)**.
  * Unos flags que permiten configurar varios aspectos relacionados con el comportamiento del temporizador que se está iniciando. Para este proyecto, se puede usar el comportamiento por defecto, que corresponde a un valor de cero en este parámetro.
* **add_timer**: permite activar el temporizador, teniendo que haber asignado previamente al campo expires la duración de la temporización.
* **mod_timer**: permite cancelar un temporizador actualmente activo y reactivarlo con una nueva duración especificada como parámetro. Si no estaba activo ese temporizador, simplemente lo activa con lo que puede servir como sustituto de add_timer eliminando la necesidad de modificar explícitamente el campo **expires**.
* **del_timer_sync**: desactiva el temporizador y, en el caso de un multiprocesador, espera a que la función asociada al temporizador concluya, en el caso de que se estuviera ejecutando en ese momento.

Es importante resaltar que la rutina del temporizador se ejecuta de manera asíncrona, en el contexto de una interrupción software. Por tanto, para protegerse de la misma en un entorno multiprocesador hay que usar spin_lock_bh para inhibir las interrupciones software en el fragmento de código conflictivo.

### Funcionalidad a desarrollar de la cuarta fase

Como se comentó previamente, esta fase es el meollo del proyecto puesto que en ella hay que implementar toda la lógica del modo de operación desacoplado del manejador.

En cuanto a la operación de escritura, dado que se asume que el manejador puede ejecutar en un sistema multiprocesador con un núcleo expulsivo, para evitar condiciones de carrera (recuerde que la estructura kfifo está diseñada para que funcione con un único productor y un único consumidor, siendo necesaria una sincronización explícita en caso contrario), hay que asegurarse de que se secuencia el procesamiento de las llamadas de escritura (como se verá más adelante, también hay que secuenciar las llamadas **fsync** tanto entre sí como con respecto a las escrituras).

Tal como se ha explicado previamente, el comportamiento de la llamada de escritura seguirá el rol de productor trabajando de forma desacoplada con el proceso de consumir los datos (la reproducción de sonidos). Esta llamada quedará bloqueada solo si se encuentra la cola llena y terminará en cuanto haya podido copiar en la cola todos los bytes que se pretenden escribir. Esta llamada, solo en caso de que en ese momento no se esté reproduciendo ningún sonido (el dispositivo está parado), programará la reproducción del sonido de turno, activando el altavoz, a no ser que se trate de un silencio. Recuerde que si el tamaño de la escritura no es múltiplo de 4, al no tratarse de un sonido completo, se guardará esa información parcial hasta que una escritura posterior la complete.

La programación del dispositivo implica fijar la frecuencia del altavoz al valor especificado y establecer un temporizador con un valor correspondiente a la duración del sonido. En caso de tratarse de un silencio no hay que fijar la frecuencia sino desactivar el altavoz si estuviera activo en ese momento.

Como se comentó previamente, este modo desacoplado conlleva algunas dificultades en lo que se refiere a la descarga del módulo (rmmod) puesto que, aunque no haya ningún fichero abierto asociado al manejador, este puede seguir procesando los sonidos almacenados en la cola. Hay que asegurarse, por tanto, en la operación de fin de módulo de que se realiza una desactivación ordenada del dispositivo, deteniendo el temporizador que pudiera estar activo (en un multiprocesador, incluso podría estar ejecutando en ese momento la rutina del temporizador) y silenciando el altavoz.

### Pruebas de la cuarta fase

A continuación se propone una serie de pruebas para verificar si la funcionalidad se ha desarrollado correctamente. Para poder verificar mejor el comportamiento de la práctica, se recomienda imprimir un mensaje a la entrada y a la salida de la función de escritura, al comienzo y al final de la función asociada al temporizador y cada que vez que se va a programar un sonido especificando la frecuencia y duración del mismo. Usando el mandato dmesg podrá visualizar todos esos mensajes. Se proporciona un fichero binario (songs.bin) con la secuencia de sonidos de varias canciones, así como una versión en modo texto del mismo (songs.txt) para facilitar la comprobación del buen comportamiento del código en las pruebas.

1. Esta prueba comprueba si se procesa correctamente un único sonido (la primera nota de la sintonía de Los Simpsons).

        $ dd if=songs.bin of=/dev/intspkr bs=4 count=1

Compruebe con dmesg que la frecuencia y duración son adecuadas (puede verificarlo comprobando los valores almacenados en la primera línea del fichero songs.txt), y que el altavoz se ha programado correctamente:

        spkr set frequency: 1047
        spkr ON
        spkr OFF

2. En esta prueba se genera el mismo sonido pero realizando dos escrituras de dos bytes. El comportamiento dede ser el mismo que la prueba anterior.

        $ dd if=songs.bin of=/dev/intspkr bs=2 count=2

3. Esta prueba genera los 8 primeros sonidos del fichero songs.bin usando 8 escrituras y dejando tanto el tamaño del buffer como del umbral en sus valores por defecto. La primera escritura debe activar el primer sonido y las restantes deben devolver el control inmediatamente. Con dmesg se debe apreciar que, exceptuando el primer sonido, los demás son programados en el contexto de la rutina de tratamiento del temporizador.

        $ dd if=songs.bin of=/dev/intspkr bs=4 count=8

4. La misma prueba que la anterior pero con una única escritura, que debe completarse inmediatamente.

        $ dd if=songs.bin of=/dev/intspkr bs=32 count=1

5. Esta prueba intenta comprobar que se tratan adecuadamente las pausas o silencios que aparecen en una secuencia. Para ello, se van a generar los 20 primeros sonidos, donde aparecen dos pausas. Debe comprobarse que el altavoz se desactiva al tratar esas pausas y que se reactiva al aparecer nuevamente sonidos convencionales en la secuencia. Se va a probar con escrituras de 4 bytes y con una única escritura:

        $ dd if=songs.bin of=/dev/intspkr bs=4 count=20
        $ dd if=songs.bin of=/dev/intspkr bs=80 count=1

6. Esta prueba va a forzar que se llene la cola pero no va a definir ningún umbral. Para llevarla a cabo, se debe cargar el módulo especificando un tamaño de buffer de 32. A continuación, se va ejecutar una prueba que genere 20 sonidos. En primer lugar, con escrituras de 4 bytes:

        dd if=songs.bin of=/dev/intspkr bs=4 count=20

Debe comprobar con dmesg como la llamada de escritura que encuentra la cola llena se bloquea y que, cada vez que completa el procesado de un sonido, se desbloquea al proceso escritor puesto que, al realizar operaciones de 4 bytes, ya tiene sitio en la cola para completar la llamada y ejecutar en paralelo con el procesamiento de los sonidos previos.

Finalizada la prueba, repítala usando una única escritura de 80 bytes.

        dd if=songs.bin of=/dev/intspkr bs=80 count=1

En este caso, debe comprobar cómo en la operación de escritura se producen solo dos bloqueos.

7. Esta prueba comprueba el funcionamiento del umbral. Para ello, se repetirá la anterior (programa que genera 20 sonidos) pero especificando un tamaño de umbral de 16 bytes. La prueba que realiza escrituras de 4 bytes no se verá afectada por el cambio pero sí lo estará la que realiza una única escritura de 80 bytes, que se bloqueará tres veces, en lugar de 2.

8. Aunque se podrían probar múltiples situaciones de error, vamos a centrarnos solo en una: la dirección del buffer de la operación write no es válida y, por tanto, esta llamada debe retornar el error -EFAULT. La prueba consiste simplemente en ejecutar el programa error proporcionado como material de apoyo.

        ./error

9. Esta es una prueba de esfuerzo (usa los valores por defecto de todos los parámetros): se reproduce todo el fichero de canciones usando escrituras de 4 bytes:

        $ dd if=songs.bin of=/dev/intspkr bs=4

y, acabada completamente esa prueba (con la cola vacía), se repite usando escrituras de 4KiB:

        $ dd if=songs.bin of=/dev/intspkr bs=4096

10. En esta prueba, que también usa los valores por defecto de todos los parámetros, se va a comprobar que cuando se descarga el módulo justo después de completarse la aplicación, se hace de forma correcta deteniendo el procesamiento de sonidos y dejando en silencio el altavoz:

        $ dd if=songs.bin of=/dev/intspkr bs=4096 count=1
        $ sleep 1
        $ rmmod spkr