# KTorrentSearch (`ktorrentsearch`) 🧲

> **Buscador de torrents multi-tracker nativo sobre `CachyOS.UI 1.0` (Golden Gate / Liquid Glass) con integración directa en `transmission-qt` y motor `nova3` autónomo.**

Diseñado específicamente para reemplazar el buscador integrado de qBittorrent al migrar a **Transmission Qt**, manteniendo el 100 % de los motores de búsqueda (`nova3`) con una huella de RAM mínima (~22 MB) y aceleración por GPU sobre Wayland (KWin 6.8).

---

## ✨ Características Principales

- **Runtime `nova3` 100 % Autónomo (Sin depender de qBittorrent):**
  - Empaqueta e instala su propio entorno `nova3` (`nova2.py`, `nova2dl.py`, `novaprinter.py`, `helpers.py`, `socks.py`) y los 11 motores de búsqueda en `~/.local/share/ktorrentsearch/nova3`.
  - Puedes desinstalar `qbittorrent` por completo (`sudo pacman -Rns qbittorrent`) sin perder ningún plugin de búsqueda.
- **Ejecución Asíncrona Multi-Proceso y Streaming en Tiempo Real:**
  - Lanza un subproceso `QProcess` independiente por cada motor habilitado con *watchdog* de 15 s, parseando resultados línea a línea (`readyReadStandardOutput`) conforme llegan y deduplicando automáticamente por hash `btih`.
- **Integración Directa con `transmission-qt` (Wayland XDG Activation):**
  - **Enlaces `magnet:?xt=...`:** Envío directo a `transmission-qt` mediante `KIO::CommandLauncherJob` (transfiriendo el token de activación Wayland a KWin para elevar la ventana de Transmission Qt al instante).
  - **Archivos `.torrent` remotos (`http://`/`https://`):** Resolución transparente mediante `nova2dl.py` (gestionando cookies, cabeceras y compresión gzip del tracker) y apertura inmediata del archivo `.torrent` temporal en `transmission-qt`.
- **Estética `CachyOS.UI 1.0` (macOS 27 Golden Gate):**
  - `CachyWindow` con desenfoque KWin Wayland (`CachyWindowBlur`), esquinas *squircle* de 20 px, barra lateral *edge-to-edge* por categorías y motores, filtrado instantáneo en memoria (`QSortFilterProxyModel`) y política estricta de **Cero Desbordes (*Zero Overflow Policy*)**.

---

## 🚀 Compilación e Instalación

```bash
cd /home/cesar/Proyectos/ktorrentsearch
./install.sh
```
