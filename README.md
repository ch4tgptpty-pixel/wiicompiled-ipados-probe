# Prueba desechable de WiiCompiled para iPadOS

Objetivo: iPad de novena generación, iPadOS 26.2.1; el usuario firma con FlareStore.
Estado: compilado correctamente con Xcode 26.6 y SDK iOS 26.5 en GitHub Actions.
**Todavía no probado en el iPad. No es un port jugable.**

Repositorio: https://github.com/ch4tgptpty-pixel/wiicompiled-ipados-probe
Build v0.2 correcto: https://github.com/ch4tgptpty-pixel/wiicompiled-ipados-probe/actions/runs/38321564083
Artefacto v0.2: https://github.com/ch4tgptpty-pixel/wiicompiled-ipados-probe/actions/runs/38321564083/artifacts/11778423006

Este repositorio pequeño permite crear una app de diagnóstico con GitHub Actions
sin un Mac propio, sin archivos del juego y sin certificados de firma en GitHub.
El resultado generado es `WiiCompiledProbe-unsigned.ipa`; el ZIP de fuentes entregado aquí
contiene fuentes y un workflow, no un IPA.

## Qué comprueba

- Metal nativo: muestra un rectángulo azul y confirma la finalización de un comando GPU.
  No prueba Aurora, Dawn, shaders de Wii ni rendimiento del juego.
- Reserva virtual: intenta reservar y liberar 4 GiB en la dirección de 512 GiB usada
  por el runtime actual. No escribe 4 GiB de RAM, no sobrescribe mappings existentes,
  La v0.2 también intenta una reserva dinámica de 4 GiB y, si funciona,
  crea aliases de una página en offsets 0x80000000 y 0xc0000000 y comprueba
  lectura y escritura compartidas. No valida el backend completo ni el juego.
  Un fallo es un resultado útil.
- Contexto: enlaza el `host_context.cpp` y `co_switch.S` originales y realiza dos
  cambios de contexto. La prueba es manual y podría cerrar la app si el backend falla.
- Auditoría de compilación: verifica la sintaxis de tres fuentes originales con el
  SDK de iOS y guarda PASS/FAIL y errores en los logs. Una auditoría fallida no se
  oculta y no impide intentar compilar la app de diagnóstico, que no usa esas fuentes.

El upstream está fijado a `bd219e38a3e9a1a5047ce41d8cd01210bef3af43` para poder
reproducir esta prueba. No modifica ni envía contribuciones al proyecto original.

## Compilar desde Windows con GitHub Actions

1. Extrae el ZIP y crea un repositorio vacío en tu cuenta de GitHub. Para utilizar
   ejecutores estándar gratuitos, el repositorio puede ser público: este paquete
   sólo contiene el diagnóstico. No añadas datos del juego.
2. Abre PowerShell en la carpeta que contiene este README y ejecuta:

   ```powershell
   git init -b main
   git add .
   git commit -m "Add disposable iPadOS feasibility probe"
   git remote add origin https://github.com/TU_USUARIO/TU_REPOSITORIO.git
   git push -u origin main
   ```

   Sustituye la URL por la del repositorio creado. Git pedirá iniciar sesión si
   corresponde. Necesitas identidad de Git configurada para crear el commit.
   También puedes subir los archivos con GitHub, conservando `.github/workflows/`.

3. En GitHub, abre **Actions → iPadOS feasibility probe → Run workflow**.
4. Espera el resultado. Descarga **ipados-probe-results** en Artifacts.
   Si falla, el artefacto contendrá los logs disponibles; no implica que exista un IPA.
5. Si el build termina correctamente, extrae `WiiCompiledProbe-unsigned.ipa`.
   Debe firmarse antes de instalarlo. Intenta importarlo en FlareStore si esa
   herramienta admite IPA sin firmar; su compatibilidad no se ha verificado aquí.
6. Abre la app, comprueba el rectángulo azul y pulsa **Probar reserva virtual**.
   Después pulsa **Probar contexto del runtime** y finalmente **Copiar informe**.
   Comparte el texto y los logs de Actions en esta conversación. Si se cierra,
   indica qué botón pulsaste. Cada resultado también se guarda dentro del sandbox.

La app no solicita entitlements especiales. Si el espacio virtual no está disponible,
habrá que estudiar otro backend de memoria o los permisos de firma aplicables.
Firmar apps ilimitadas no confirma acceso a un espacio virtual ampliado.

## Límites

- Una compilación exitosa de este diagnóstico no demuestra que el juego funcione.
- Aún falta adaptar CMake, compilar Dawn/SDL para iOS, separar servicios macOS,
  adaptar el arranque y los archivos al sandbox y probar los gráficos reales.
- Para integrar el juego posteriormente se necesitan los datos y fuentes generados
  a partir de la copia propia PAL RMCP01. No se incluye ni solicita subir esos datos.
- El repositorio separado se creó y el diagnóstico compiló correctamente.
  La auditoría confirmó que `mach_vm.h` no está soportado en el SDK de iOS
  y que `CoreAudio/CoreAudio.h` del backend macOS no está disponible.
- El SDK instalado en el runner determina qué versiones de iPadOS puede compilar;
  el deployment target del diagnóstico es iOS 16.0, compatible como mínimo solicitado.

## Licencia

El diagnóstico se distribuye bajo GPL-3.0, por su integración con fuentes GPL-3.0
de WiiCompiled. Véase LICENSE. Los frameworks de Apple son dependencias del SDK.
Proyecto original: https://github.com/patchzyy/Wiicompiled

