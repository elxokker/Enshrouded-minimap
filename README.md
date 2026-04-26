# Embervale Minimap

Minimapa integrado en el juego para Enshrouded usando Shroudtopia.

No es un overlay externo de Windows. El mod se engancha al cliente del juego y
dibuja el minimapa dentro del propio frame de Enshrouded.

## Que Hace

- Muestra un minimapa circular en la parte superior derecha.
- Usa un aro visual premium y assets incluidos en el mod.
- Renderiza el mapa real de Embervale en pequeno, no un mapa inventado.
- Muestra la posicion y orientacion del jugador.
- Usa iconos reales extraidos del mapa del juego.
- Muestra puntos de interes cercanos visibles/detectados por el mapa.
- Usa datos de fog-of-war y POIs como fallback cuando el juego no expone todos
  los marcadores en vivo.
- Se integra en el render Vulkan del juego, sin ventana externa.

## Controles

- `+` aumenta el zoom del minimapa.
- `-` reduce el zoom del minimapa.

Tambien funcionan `+` y `-` del teclado numerico.

## Descargar Release

Descarga el zip desde la ultima release:

[Embervale Minimap v0.4.34](https://github.com/elxokker/embervale-minimap/releases/tag/v0.4.34)

Archivo:

`embervale-minimap-v0.4.34.zip`

## Instalar el Zip

1. Cierra Enshrouded.
2. Descarga `embervale-minimap-v0.4.34.zip`.
3. Extrae el zip.
4. Copia la carpeta `minimap_mod` completa a:

```text
C:\Program Files (x86)\Steam\steamapps\common\Enshrouded\mods\minimap_mod
```

La carpeta final debe quedar asi:

```text
Enshrouded
└─ mods
   └─ minimap_mod
      ├─ minimap_mod.dll
      ├─ mod.json
      ├─ embervale_minimap_frame.rgba
      ├─ embervale_minimap_icons.bin
      ├─ embervale_player_arrow.rgba
      ├─ embervale_realmap_1280.rgba
      └─ ...
```

5. Arranca Enshrouded con Shroudtopia.

Si ya tenias una version anterior, reemplaza la carpeta `minimap_mod` completa
por la nueva.

## Build Desde Codigo

Requisitos:

- Windows.
- Visual Studio con MSBuild y toolset C++.
- Shroudtopia instalado en el juego para cargar el mod.

Compilar:

```powershell
.\build-release.ps1
```

Instalar en la ruta por defecto de Steam:

```powershell
.\install.ps1
```

Instalar en otra ruta:

```powershell
.\install.ps1 -GameDir "D:\SteamLibrary\steamapps\common\Enshrouded"
```

## Estructura

- `src/` - codigo del mod.
- `include/` - headers minimos de Shroudtopia usados para compilar.
- `assets/` - assets que se copian junto al DLL.
- `build-release.ps1` - build `Release|x64`.
- `install.ps1` - instala el mod y crea backup de la instalacion anterior.

## Version

Version actual del mod: `0.4.34`.

Este mod apunta a una build concreta de Enshrouded/Shroudtopia. Si el juego se
actualiza, puede ser necesario revisar offsets y hooks.
