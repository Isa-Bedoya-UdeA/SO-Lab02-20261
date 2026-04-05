# SO - Lab02 - 20261

## Equipo

* Rafael Angel Alemán Castillo. [rafael.aleman@udea.edu.co](mailto:rafael.aleman@udea.edu.co). CC. 1001560844
* Isabela Bedoya Gaviria. [isabela.bedoya@udea.edu.co](mailto:isabela.bedoya@udea.edu.co). CC. 1020106520

---

## Cómo ejecutar

### Requisitos previos

* Sistema operativo Linux (o WSL en Windows).
* GCC instalado (`gcc --version`).
* `make` instalado (`make --version`).
* Bash disponible (`bash --version`).

### Pasos para compilar y ejecutar el shell

```bash
# 1. Clonar o descomprimir el repositorio y entrar a la carpeta raíz
cd SO-Lab02-20261

# 2. Compilar el proyecto
make

# 3. Ejecutar wish en modo interactivo
./wish

# 4. Ejecutar wish en modo batch (archivo de comandos)
./wish mi_archivo.txt

# 5. Limpiar los binarios compilados
make clean
```

### Pasos para ejecutar el script de pruebas

```bash
# Desde la raíz del proyecto (donde están Makefile y test-wish.sh)
chmod +x test-wish.sh
./test-wish.sh
```

El script compila el proyecto automáticamente, ejecuta todas las pruebas y muestra un resumen con colores al final.

---

## Documentación

### Estructura del proyecto

```
lab02-wish/
├── src/
│   ├── shell.h       # Tipos compartidos, constantes y prototipos
│   ├── parser.c      # División de líneas y tokenización de comandos
│   ├── builtins.c    # Comandos integrados: exit, cd, path
│   ├── executor.c    # Fork/exec, resolución de paths, redirección, paralelismo
│   └── wish.c        # Punto de entrada: loop principal del shell
├── Makefile          # Compilación modular del proyecto
├── test-wish.sh      # Script de pruebas automatizadas
└── README.md         # Este archivo
```

La solución está organizada en cuatro archivos `.c` con responsabilidades separadas, unidos por el header `shell.h`. Esta separación hace que cada módulo sea legible, probable y modificable de forma independiente.

---

### `shell.h` — Definiciones compartidas

Header central incluido por todos los módulos. Define:

| Elemento | Descripción |
|---|---|
| `MAX_SEARCH_DIRS` | Máximo de directorios en el search path (64) |
| `MAX_TOKENS` | Máximo de tokens en un comando (128) |
| `MAX_PARALLEL` | Máximo de comandos paralelos por línea (64) |
| `PATH_BUF_SIZE` | Tamaño del buffer para rutas completas (512) |
| `ERROR_MSG` / `ERROR_LEN` | Mensaje de error único exigido por el enunciado |
| `Command` (struct) | Representación de un comando ya parseado: `argv[]`, `argc`, `output_file` |
| `search_dirs[]` / `dir_count` | Estado global del search path (definido en `builtins.c`) |
| `emit_error()` | Función inline: escribe el mensaje de error a `stderr` |
| `strip_whitespace()` | Función inline: elimina espacios/tabs/newlines al inicio y fin de una cadena |

---

### `parser.c` — Análisis de entrada

Módulo puramente textual: no hace fork, ni exec, ni llama al sistema operativo.

#### `int split_parallel(char *line, char *out[], int max_out)`

Divide la línea de entrada usando `&` como separador mediante `strsep()`. Cada segmento no vacío se limpia con `strip_whitespace()` y se copia con `strdup()` en `out[]`. Retorna el número de segmentos encontrados.

#### `static int validate_redirection(const char *target)`

Función auxiliar privada. Verifica que el destino de una redirección sea exactamente un token: sin `>` adicional, sin espacios embebidos y no vacío. Retorna 1 si es válido, 0 si no.

#### `int parse_command(char *raw_cmd, Command *cmd)`

Toma un segmento de comando (sin `&`) y lo convierte en un `Command`. Detecta y valida la redirección `>` partiendo la cadena en ese símbolo; luego tokeniza la parte izquierda con `strtok()` para llenar `cmd->argv[]` y `cmd->argc`. Retorna 0 en éxito, -1 en error de parseo (el error ya fue emitido).

---

### `builtins.c` — Comandos integrados

Posee el estado global `search_dirs[]` y `dir_count`, e implementa los tres comandos integrados requeridos.

#### `void path_init(void)`

Inicializa el search path con un único directorio `/bin`, como exige el enunciado. Se llama una sola vez al arrancar el shell.

#### `static void handle_exit(const Command *cmd, int *should_exit)`

Valida que no haya argumentos. Si `argc != 1` emite error sin terminar. Si la llamada es válida, pone `*should_exit = 1` para que el loop principal detenga la ejecución limpiamente después de esperar a todos los hijos paralelos.

#### `static void handle_cd(const Command *cmd)`

Acepta exactamente un argumento. Con cero o más de uno emite error. Llama a `chdir()` y emite error si falla.

#### `static void handle_path(const Command *cmd)`

Libera todos los directorios del search path actual y los reemplaza con los argumentos de `argv[1]` en adelante. Con cero argumentos el path queda vacío y ningún comando externo puede ejecutarse.

#### `int builtin_dispatch(const Command *cmd, int *should_exit)`

Punto de entrada público del módulo. Compara `cmd->argv[0]` con cada nombre de built-in y delega a la función correspondiente. Retorna 1 si el comando era un built-in (para que el caller no haga fork), 0 si es externo.

---

### `executor.c` — Ejecución de procesos

#### `static char *resolve_executable(const char *name, char *buf, size_t buf_sz)`

Recorre `search_dirs[]` construyendo la ruta `<dir>/<name>` en `buf`. Usa `access(X_OK)` para verificar ejecutabilidad. Retorna `buf` con la primera ruta válida encontrada, o `NULL` si no existe en ningún directorio.

#### `static int setup_redirection(const char *filepath)`

Llamada únicamente dentro del proceso hijo. Abre el archivo destino con `O_WRONLY | O_CREAT | O_TRUNC` y redirige tanto `stdout` como `stderr` hacia él con `dup2()`. Retorna 0 en éxito, -1 si falla.

#### `pid_t run_command(const Command *cmd)`

Hace `fork()`. El hijo configura la redirección si aplica, resuelve el ejecutable con `resolve_executable()` y llama a `execv()`. El padre **no llama a `wait()` aquí**: devuelve el PID del hijo al caller. Este diseño permite lanzar múltiples hijos antes de esperar a cualquiera, habilitando el paralelismo real con `&`.

#### `int run_line(char *line)`

Orquestador principal de una línea de entrada. Llama a `split_parallel()` para obtener los segmentos, luego itera: parsea cada uno con `parse_command()`, lo despacha como built-in con `builtin_dispatch()` o lo lanza como externo con `run_command()`, acumulando los PIDs. Solo después de lanzar **todos** los procesos llama a `waitpid()` por cada PID. Retorna 1 si el shell debe terminar, 0 si continúa.

---

### `wish.c` — Punto de entrada

El `main()` del shell. Valida los argumentos (0 o 1 permitidos), abre el archivo batch si corresponde, llama a `path_init()`, e inicia el loop read-eval:

1. Imprime `wish> ` solo en modo interactivo.
2. Lee una línea con `getline()`.
3. Limpia la línea con `strip_whitespace()` e ignora líneas vacías.
4. Pasa la línea a `run_line()`.
5. Sale si `run_line()` retorna 1 o si `getline()` llega a EOF.

Al finalizar libera el buffer de línea y el search path.

---

## Pruebas

Las pruebas se encuentran en `test-wish.sh`. El script corre en Bash y cubre todos los requisitos del enunciado. Usa colores (`GREEN ✔` / `RED ✘` / `YELLOW ⚠`) para facilitar la lectura de la salida en consola.

### Secciones del script

| Sección | Qué verifica |
|---|---|
| 1. Compilación | `make` compila sin errores y produce `./wish` ejecutable |
| 2. Batch mode | Comandos desde archivo, ausencia de prompt, EOF, errores de argumentos |
| 3. Comandos externos | `ls`, `echo`, comando inexistente, múltiples líneas secuenciales |
| 4. Built-in exit | Salida limpia, error con argumentos, continuación tras error |
| 5. Built-in cd | Cambio exitoso, sin args, demasiados args, directorio inexistente, continuación |
| 6. Built-in path | Path por defecto, `/usr/bin`, path vacío, reemplazo (no append), múltiples dirs |
| 7. Redirección | Escritura en archivo, truncación, sin output en terminal, sin espacios, casos de error |
| 8. Comandos paralelos | Dos y tres comandos, espera antes de siguiente línea, `&` sin espacios, mezcla bueno/malo |
| 9. Edge cases | Whitespace extra, tabs, `cd` + comando, ciclo completo de path |
| 10. Limpieza | `make clean` elimina el binario |

Al final el script imprime el total de pruebas pasadas y fallidas, y retorna como código de salida el número de fallas (0 = todo correcto).

---

## Problemas

No se presentaron problemas durante la solución del presente laboratorio.

---

## Video

[https://drive.google.com/file/d/1rX1mO0a6O7G_KT0vi52EJnx2GiiSz0b0/view?usp=sharing](https://drive.google.com/file/d/1rX1mO0a6O7G_KT0vi52EJnx2GiiSz0b0/view?usp=sharing)

---

## Manifiesto de transparencia

Se utilizó IA generativa (Claude Sonnet 4.6) en los siguientes puntos del desarrollo:

* **Creación del test:** Los casos de prueba se diseñaron manualmente, pero se usó la IA para reunirlos en un solo `.sh` con formato para mejor visualización de los resultados.
* **Redacción del archivo README.md:** Las secciones de *Cómo ejecutar*, *Documentación* y *Pruebas* fueron generadas con IA a partir del código del laboratorio.
* **Comentarios y documentación:** Se usó IA generativa para añadir comentarios en los archivos del laboratorio para mejor documentación del proyecto a partir del código creado manualmente.
