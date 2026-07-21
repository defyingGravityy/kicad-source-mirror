# GSEIM-Eeschema Standalone

A lightweight standalone build of KiCad's Eeschema with integrated GSEIM simulation support.

This build removes the PCB editor and other unrelated KiCad applications, leaving only the schematic editor and GSEIM simulation functionality.

## Requirements

- Linux
- CMake (>= 3.25 recommended)
- Ninja
- C++ compiler with C++20 support (GCC or Clang)
- wxWidgets
- All normal KiCad build dependencies required for Eeschema

## Clone

```bash
git clone <repository-url>
cd kicad-source-mirror
```

## Configure

```bash
cmake -B build-ninja -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PWD" \
    -DDEFAULT_INSTALL_PATH="$PWD" \
    -DKICAD_DATA="$PWD/share/kicad" \
    -DKICAD_LIBRARY_DATA="$PWD/share/kicad" \
```

## Build

```bash
ninja -C build-ninja
```

## Install

```bash
ninja -C build-ninja install
```

The installation will be placed in:

```text
the current working directory
```

## Run

```bash
cd "$PWD/bin"

./eeschema
```

or simply

```bash
$HOME/Desktop/GSEIM-Eeschema/bin/eeschema
```

## Build Directory

```
build-ninja/
```

contains all generated build files and may be safely deleted and regenerated at any time.

## GSEIM Symbol Library — where it lives

The GSEIM symbol library is installed at:
<install-prefix>/share/kicad/resources/gseim/GSEIM_Library.kicad_sym

For example, with:

```bash
cmake -B build-ninja -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Desktop/GSEIM-Eeschema"
```

the library table entry points at:
~/Desktop/GSEIM-Eeschema/share/kicad/resources/gseim/GSEIM_Library.kicad_sym

This is the file KiCad's symbol editor writes to when you create or edit
GSEIM symbols. It is **not** a separate user-data directory — it's the
same file that gets installed by `cmake --install`.

### This file is overwritten on reinstall

Any symbols you add live directly inside the install prefix. If you
re-run `cmake --install build-ninja` (or delete and rebuild the install
prefix), this file is replaced with the pristine copy from the source
tree, and anything not committed to source control is lost.

**Before reinstalling, back up your symbols:**

```bash
cp ~/Desktop/GSEIM-Eeschema/share/kicad/resources/gseim/GSEIM_Library.kicad_sym \
   ~/gseim_symbols_backup.kicad_sym
```

**After reinstalling, restore them:**

```bash
cp ~/gseim_symbols_backup.kicad_sym \
   ~/Desktop/GSEIM-Eeschema/share/kicad/resources/gseim/GSEIM_Library.kicad_sym
```

If a symbol is meant to be part of the shipped default set rather than a
personal/local addition, add it to `resources/gseim/GSEIM_Library.kicad_sym`
in the source tree and commit it instead of relying on the manual
backup/restore above.

## Cleaning

To rebuild from scratch:

```bash
rm -rf build-ninja

cmake -B build-ninja -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Desktop/GSEIM-Eeschema" \

ninja -C build-ninja
ninja -C build-ninja install
```

## Features

- Standalone Eeschema application
- Integrated GSEIM simulator
- GSEIM netlist export
- Electrical Building Blocks (EBEs)
- XBE support
- Multiple solve blocks
- GSEIM parameter editor
- Out-variable selection

## Notes

This project builds only the standalone schematic editor. The following KiCad applications are intentionally excluded:

- PCB Editor (Pcbnew)
- GerbView
- Bitmap2Component
- CvPcb
- PCB Calculator
- 3D Viewer
- Page Layout Editor
- QA tools


## GSEIM Source Files

The GSEIM integration consists of the following source files and resources.

### Netlist Export

```
eeschema/netlist_exporters/
├── netlist_exporter_gseim.cpp
└── netlist_exporter_gseim.h
```

Implements the GSEIM netlist exporter and generates `.cir` files directly from Eeschema schematics.

---

### GSEIM Component Database

```
eeschema/gseim/
├── gseim_component_db.cpp
├── gseim_component_db.h
├── gseim_ebe_parser.cpp
└── gseim_ebe_parser.h
```

Loads and parses GSEIM Electrical Building Block (EBE) and XBE descriptions.

---

### Solver Parameter Database

```
eeschema/gseim/
├── gseim_solver_parameter_db.cpp
└── gseim_solver_parameter_db.h
```

Loads solver parameters from `slvparams.in` and provides them to the UI.

---

### Subcircuit Database

```
eeschema/gseim/
├── gseim_subckt_db.cpp
└── gseim_subckt_db.h
```

Scans the GSEIM subcircuit directory and makes available user-defined subcircuits.

---

### User Interface

```
eeschema/dialogs/
├── dialog_gseim_parameters.cpp
└── dialog_gseim_parameters.h
```

Provides the GSEIM parameter editor for configuring EBE/XBE parameters.

Additional GSEIM controls have been integrated into:

```
eeschema/dialogs/
├── dialog_export_netlist.cpp
├── dialog_export_netlist.h
├── panel_export_netlist.cpp
└── panel_export_netlist.h
```

These provide:

- Solve block configuration
- Solver parameter editing
- Output variable selection
- Netlist export options

---

### Resources

```
resources/gseim/
├── bin/
├── ebe/
├── xbe/
├── subckt/
├── slvparams.in
├── GSEIM_Library.kicad_sym
└── ...
```

Contains:

- EBE,XBE,Subcircuit models
- Solver parameter definitions
- GSEIM component library
- Executables and runtime resources


---

### Additional files

```
eeschema/tools/
├── sch_edit_tool.cpp
├── sch_edit_tool.h
├── sch_actions.cpp
├── sch_actions.h
```

Contains Functions for:

- Modify ebe/xbe/subcircuit parameters
- Select ebe/xbe/subcircuit Output variables
- Run simulation program
- Run plotting program

---

### Standalone Build

```
CMakeLists.txt
common/CMakeLists.txt
eeschema/CMakeLists.txt
```
