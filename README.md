# 🐚 Proyecto Shell `wish` — API de Procesos
### Laboratorio de Sistemas Operativos — Práctica No. 2
**Universidad de Antioquia | Facultad de Ingeniería | Ingeniería de Sistemas**

---

## 👥 Integrantes

| Nombre completo | Correo | Documento |
|----------------|--------|-----------|
| Juan C. ...    | @udea.edu.co | |
| ...            | @udea.edu.co | |

---

## 📋 Descripción general

`wish` (*Wisconsin Shell*) es un intérprete de comandos (CLI) simple implementado en C, inspirado en el shell de Unix. El shell opera en un ciclo infinito mostrando el prompt `wish> `, lee un comando del usuario, lo ejecuta y espera su finalización. Este proceso se repite hasta que el usuario escribe `exit`.

El shell soporta dos modos de operación:
- **Modo interactivo:** el usuario escribe comandos directamente en la terminal.
- **Modo batch:** se pasa un archivo de texto como argumento; el shell ejecuta cada línea como un comando sin mostrar el prompt.

---

## 📁 Estructura del repositorio

```
LAB2_SISTEMAS_OPERATIVOS/
├── src/
│   ├── wish.c       # Código fuente principal del shell
│   └── Makefile     # Script de compilación
└── README.md        # Este archivo
```

---

## ⚙️ Compilación

Requisitos: `gcc`, `make` (disponibles en cualquier sistema Linux o WSL).

```bash
cd src
make
```

Esto genera el ejecutable `wish` en la carpeta `src/`. Para limpiar:

```bash
make clean
```

---

## 🚀 Uso

**Modo interactivo:**
```bash
./wish
wish> ls -la
wish> cd /tmp
wish> exit
```

**Modo batch:**
```bash
./wish batch.txt
```

**Error (más de un argumento):**
```bash
./wish a b   # Imprime error y termina
```

---

## 🔧 Documentación de funciones

### `void print_error(void)`
Imprime el mensaje de error estándar `"An error has occurred\n"` hacia `stderr` usando `write()`, tal como lo especifica el enunciado. Es la única salida de error del shell.

---

### `void init_path(void)`
Inicializa el *search path* del shell con un único directorio por defecto: `/bin`. Se llama una sola vez al iniciar el programa.

---

### `void free_path(void)`
Libera toda la memoria dinámica asociada al arreglo `search_path`. Se llama al cambiar el path con `set_path()` o al terminar el programa.

---

### `void set_path(char **new_dirs, int count)`
Reemplaza el *search path* actual con los nuevos directorios recibidos como argumento. Si `count == 0`, el path queda vacío y el shell no puede ejecutar ningún comando externo (solo built-ins).

---

### `char *find_executable(const char *cmd)`
Busca el ejecutable correspondiente al comando `cmd` dentro de los directorios del *search path*, usando `access(path, X_OK)`. Si el comando contiene `/`, se trata como ruta absoluta o relativa y se verifica directamente. Retorna la ruta completa si la encuentra, o `NULL` en caso contrario.

---

### `char **tokenize(char *input, int *count)`
Divide la cadena `input` en tokens, tratando `>` como un token independiente. Maneja correctamente casos como `ls>file` (sin espacios alrededor del operador de redirección). Retorna un arreglo de strings terminado en `NULL` y actualiza `*count` con la cantidad de tokens.

---

### `int parse_command(char *cmd_str, char ***argv_out, char **redir_file)`
Analiza un comando individual. Usa `tokenize()` para separar el comando en piezas, identifica si hay redirección (`>`), valida que no haya redirecciones múltiples ni archivos extra después del nombre de destino, y construye el arreglo `argv` listo para pasarlo a `execv()`. Retorna `0` si el parsing es exitoso o `-1` si hay un error de sintaxis.

---

### `char **split_parallel_commands(char *line, int *out_count)`
Divide la línea de entrada usando `&` como separador, generando un arreglo de subcomandos. Ignora fragmentos vacíos (por ejemplo, un `&` al final de la línea). Actualiza `*out_count` con el número real de comandos.

---

### `int is_builtin(const char *name)`
Función auxiliar que retorna `1` si el nombre del comando es un built-in (`exit`, `cd`, `chd`, `path`, `route`), o `0` en caso contrario.

---

### `int process_command(char *cmd_str, int parallel)`
Orquesta la ejecución de un comando individual. Hace trim del whitespace, llama a `parse_command()`, determina si el comando es un built-in o externo, y actúa en consecuencia. Si `parallel == 1`, no espera la finalización del proceso hijo (el wait lo maneja `run_parallel_commands`).

**Built-ins implementados:**

| Comando | Alias | Comportamiento |
|---------|-------|----------------|
| `exit` | — | Llama a `exit(0)`. Error si recibe argumentos. |
| `cd` | `chd` | Cambia de directorio con `chdir()`. Requiere exactamente un argumento. |
| `path` | `route` | Reemplaza el search path con los directorios indicados. Acepta cero o más argumentos. |

---

### `void run_parallel_commands(char *line)`
Función principal de ejecución. Divide la línea con `split_parallel_commands()`. Si hay un solo comando, lo pasa directamente a `process_command()`. Si hay múltiples comandos (separados por `&`), verifica que ninguno sea un built-in (lo cual sería un error), hace `fork()` para cada uno simultáneamente y luego espera a todos con `waitpid()`.

---

### `void shell_loop(FILE *input, int interactive)`
Bucle principal del shell. Usa `getline()` para leer líneas de entrada. En modo interactivo imprime `wish> ` antes de cada lectura. Al encontrar EOF llama a `exit(0)`. Cada línea leída se pasa a `run_parallel_commands()`.

---

### `int main(int argc, char *argv[])`
Punto de entrada. Inicializa el path con `init_path()`. Si no recibe argumentos, invoca `shell_loop(stdin, 1)` (modo interactivo). Si recibe un argumento, abre el archivo y llama a `shell_loop(batch, 0)` (modo batch). Cualquier otro número de argumentos produce error y termina con `exit(1)`.

---

## ⚠️ Problemas presentados y soluciones

### 1. Parsing de redirección sin espacios (`ls>file`)
**Problema:** el tokenizador original basado en `strtok_r` no separaba correctamente el operador `>` cuando estaba pegado al comando o al nombre de archivo.  
**Solución:** se reescribió el tokenizador (`tokenize()`) con un recorrido carácter a carácter que trata `>` siempre como token independiente, independientemente de los espacios adyacentes.

### 2. Comandos paralelos con built-ins
**Problema:** ejecutar un built-in como parte de un comando paralelo (`cd /tmp & ls`) genera comportamiento indefinido porque los built-ins modifican el estado del proceso padre (directorio, path).  
**Solución:** antes de lanzar cualquier proceso en paralelo, se verifica si algún subcomando es un built-in. Si lo es, se imprime el mensaje de error y se cancela toda la ejecución.

### 3. `&` al final de la línea genera comando vacío
**Problema:** una entrada como `ls &` generaba un comando vacío al final del arreglo, causando comportamiento inesperado.  
**Solución:** `split_parallel_commands()` filtra explícitamente los fragmentos vacíos o que solo contienen whitespace antes de agregarlos al arreglo.

### 4. Mensaje de error no cumplía la especificación
**Problema:** el código original usaba `fprintf(stderr, ...)`, pero el enunciado exige usar `write(STDERR_FILENO, ...)`.  
**Solución:** se centralizó todo el manejo de errores en la función `print_error()` que usa `write()` directamente.

### 5. Manejo de memoria dinámica
**Problema:** múltiples rutas de error en `parse_command()` y `run_parallel_commands()` podían dejar memoria sin liberar.  
**Solución:** se utilizó un label `goto parse_error` para centralizar la liberación de memoria en caso de error dentro de `parse_command()`, y se verificó cada ruta de salida en `run_parallel_commands()`.

---

## 🧪 Pruebas realizadas

Todas las pruebas se ejecutaron con el comando:
```bash
echo "COMANDO" | ./wish
# o directamente en modo interactivo
```

| # | Comando de prueba | Resultado esperado | ✅ |
|---|-------------------|-------------------|---|
| 1 | `ls` | Lista el directorio actual | ✅ |
| 2 | `ls -la /tmp` | Lista detallada de /tmp | ✅ |
| 3 | `pwd` | Muestra directorio actual | ✅ |
| 4 | `echo hola mundo` | Imprime `hola mundo` | ✅ |
| 5 | `ls -la /tmp > salida.txt` | Crea archivo con la salida | ✅ |
| 6 | `ls>/tmp/s.txt` | Redirección sin espacios | ✅ |
| 7 | `ls > a > b` | Error (doble redirección) | ✅ |
| 8 | `sleep 1 & sleep 1` | Ambos terminan en ~1 seg | ✅ |
| 9 | `cd /tmp` luego `pwd` | Muestra `/tmp` | ✅ |
| 10 | `cd` (sin args) | Mensaje de error | ✅ |
| 11 | `cd a b` (dos args) | Mensaje de error | ✅ |
| 12 | `path /usr/bin /bin` | Cambia el search path | ✅ |
| 13 | `path` (vacío) luego `ls` | Error: no encuentra `ls` | ✅ |
| 14 | `exit` | Sale del shell | ✅ |
| 15 | `exit algo` | Mensaje de error | ✅ |
| 16 | `comandoinexistente` | Mensaje de error | ✅ |
| 17 | `./wish batch.txt` | Ejecuta comandos del archivo | ✅ |
| 18 | `./wish a b` | Error y termina con exit(1) | ✅ |
| 19 | `cd /tmp & ls` | Error (built-in en paralelo) | ✅ |
| 20 | `ls &` (& al final) | Ejecuta `ls` normalmente | ✅ |

---

## 🎥 Video de sustentación

[🔗 Enlace al video (10 minutos) — agregar antes de entregar]

---

## 🤖 Manifiesto de transparencia — Uso de IA generativa

En el desarrollo de esta práctica se utilizó IA generativa (Claude de Anthropic) en los siguientes puntos:

- **Esqueleto inicial del código:** se generó la estructura base del shell con las funciones principales.
- **Depuración del tokenizador:** la IA ayudó a identificar el bug en el parsing de `>` sin espacios y propuso la reescritura de `tokenize()`.
- **Corrección del mensaje de error:** la IA señaló que `fprintf` no cumplía la especificación del enunciado y propuso usar `write()`.
- **Manejo de casos borde:** la IA sugirió los casos de `&` al final de línea y built-ins en paralelo.
- **Redacción de la documentación:** este README fue estructurado con apoyo de la IA y revisado por los integrantes del grupo.

Todo el código fue revisado, comprendido y validado por los integrantes antes de la entrega.