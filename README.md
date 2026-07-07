# KiCad GSEIM Integration

Native GSEIM simulation support for **KiCad Eeschema**.

This project extends KiCad by adding a complete export pipeline for the **General Simulation Environment (GSEIM)**, allowing circuits designed in Eeschema to be exported directly as valid GSEIM `.cir` and `.sub` files without manual netlist editing.

---

# Features

- Native GSEIM netlist exporter
- Hierarchical subcircuit export (`.sub`)
- Automatic `.ebe` component parsing
- Automatic `.sub` parsing
- Dynamic parameter system
- Multiple solve block support
- AC / DC / Startup / Transient simulations
- Output variable manager
- Global parameter editor
- Hierarchical sheet parameter editor
- Automatic solver parameter loading from `slvparams.in`

---

# Repository Structure

```
eeschema/
│
├── dialogs/
│   └── dialog_export_netlist.cpp
│
├── gseim/
│   ├── gseim_component_db.*
│   ├── gseim_ebe_parser.*
│   ├── gseim_param_parser.*
│   ├── gseim_sub_parser.*
│   └── gseim_subckt_db.*
│
├── netlist_exporters/
│   ├── netlist_exporter_gseim.*
│   └── netlist_generator.cpp
│
├── tools/
│   └── sch_edit_tool.cpp
│
├── schematic.*
└── sch_sheet.*
```

---

# Important Files

## `netlist_exporter_gseim.*`

Main exporter responsible for generating

- `.cir`
- `.sub`

files from KiCad schematics.

---

## `gseim_ebe_parser.*`

Parses GSEIM component definitions (`.ebe`).

Extracts

- nodes
- rparms
- iparms
- sparms
- stparms
- output variables
- AC output variables

---

## `gseim_component_db.*`

Loads every `.ebe` file into an in-memory component database used by the exporter and parameter editor.

---

## `gseim_sub_parser.*`

Parses generated `.sub` files.

Used to expose subcircuit parameters inside KiCad.

---

## `gseim_subckt_db.*`

Database of all available GSEIM subcircuits.

---

## `gseim_param_parser.*`

Converts

```
r=10k c=1u v0=2
```

into

```cpp
std::map<wxString, wxString>
```

and back.

---

## `dialog_export_netlist.cpp`

Implements two new export pages

- Export as GSEIM
- Export as GSEIM Subcircuit

Features include

- Solve block editor
- Output variable editor
- Global parameter editor
- Subcircuit parameter editor

---

## `sch_edit_tool.cpp`

Adds

```
Right Click
    → Modify GSEIM Parameters
```

for

- Components
- Hierarchical sheets

---

# Export Pipeline

```
KiCad schematic
        │
        ▼
Read schematic hierarchy
        │
        ▼
Load .ebe database
        │
        ▼
Load .sub database
        │
        ▼
Collect component parameters
        │
        ▼
Collect hierarchy parameters
        │
        ▼
Generate solve blocks
        │
        ▼
Generate output variables
        │
        ▼
Write .cir / .sub
```

---

# Usage

## 1. Create a GSEIM Component

Assign

```
Gseim.Type
```

and

```
Gseim.Params
```

fields to a schematic symbol.

Example

```
Gseim.Type = r

Gseim.Params = r=10k
```

---

## 2. Modify Parameters

Right-click

```
Component
    → Modify GSEIM Parameters
```

or

```
Hierarchical Sheet
    → Modify GSEIM Parameters
```

The dialog is automatically generated from the parsed database.

---

## 3. Export a Circuit

```
File
    → Export
        → GSEIM
```

Configure

- Global parameters
- Solve blocks
- Output variables

and export a `.cir` file.

---

## 4. Export a Subcircuit

Open the hierarchical schematic.

```
File
    → Export
        → GSEIM Subcircuit
```

Configure

- Parameter defaults
- Output variables

and export a reusable `.sub`.

---

# Example Output

```text
begin_circuit

   subckt name=OA1 type=opamp
+    plus=plus
+    minus=minus
+    out=out
+    ground=0
+    a_v=1e5
+    r_in=1e6

   eelement name=R1 type=r p=0 n=minus r=10k
   eelement name=R2 type=r p=minus n=out r=22.1k

   ref_node=0

end_circuit
```

---

# Supported Simulation Types

- Startup
- DC
- Transient
- AC

Multiple solve blocks can be defined within the same project.

---

# Supported Parameter Types

Component Parameters

- Real parameters
- Integer parameters
- String parameters
- Special parameters

Global Parameters

- rparms
- iparms
- sparms
- Embedded C code

Subcircuit Parameters

- rparms
- iparms
- sparms

---

# Build

Configure and build KiCad normally.

```bash
cmake -B build-ninja -G Ninja
ninja -C build-ninja
```

Run Eeschema from the build directory.

```bash
KICAD_RUN_FROM_BUILD_DIR=1 \
KICAD_CONFIG_HOME=~/kicad-dev-config \
./build-ninja/eeschema/eeschema
```

---

# Future Work

- Direct GSEIM execution from Eeschema
- Waveform viewer integration
- Better validation of simulation parameters
- Additional GSEIM device support
- Improved diagnostics and error reporting

---

# Acknowledgements

- KiCad Development Team
- GSEIM Project