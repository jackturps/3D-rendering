# 3D Rendering

## Overview

This project is an exploration of 3D rendering using
openGL. The project is capable of parsing `*.gltf` filez
and rendering the 3D objects they describe. They can 
also apply transforms to these objects over time such
as rotation or translation.

The project is an ongoing work in progress.

## Building and Executing

The projects build system can be setup by running:
```
mkdir build
cd build
cmake ..
```

After that the project can be compiled by running:
```
make
```

and executed by running:
```
./main
```

## TODO List
* Remove hardcoded paths for model input files and take them as program arguments.
* Break out the independent code in main.c into its own files.