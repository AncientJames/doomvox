# DOOMVOX

Doom on a volumetric display.

This is a port of Doom to [Multivox](https://github.com/AncientJames/multivox). It runs either on a physical volumetric display, or in the simulator, `virtex`.

To build it,
```
cd src
make
```

To run,
```
doomvox/doomvox -iwad <wadfile>
```

You can download the wad file from (https://www.wad-archive.com/)

This will run the game with little billboard sprites running around. If you want the voxel models, download Cheello's voxel mod (https://www.moddb.com/mods/doom-voxel-project/addons/voxel-doom) and copy the `voxels/` directory into the same directory you're running `doomvox` from.


