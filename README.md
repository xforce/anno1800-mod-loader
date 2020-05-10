# Anno 1800 Mod Loader

The one and only mod loader for Anno 1800, supports loading of unpacked RDA files, XML auto merging and DLL based mods.

No file size limit. No more repacking. Less likely to break after updates (in general a mod should continue to work after every update, YMMV). 

This Changes the Games XML Files using XPath, this makes it easy and possible to only have the changes in a mod that you absolutely need instead of handling megabytes of XML files.

# Installation

Short shitty video to show how easy it is to install the loader.
> Mods have to be installed seperately.

<a href="https://files.guettler.space/98e3009f-1232-4705-b2a0-5936bd7ba477.mp4" target="_blank" title="Watch the video"><img src="https://files.guettler.space/98e3009f-1232-4705-b2a0-5936bd7ba477.jpeg" alt="Watch the video" /></a>

Head over to the releases page and download the loader.zip from the latest release.  
Unzip the contents to the location where Anno1800.exe is

> Uplay default path is `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\Bin\Win64`)

You will be asked to overwrite python35.dll, just accept that.

You probably also need the VS 2019 Redist https://aka.ms/vs/16/release/VC_redist.x64.exe

And that's basically it.

Mods will be loaded alphabetically from `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\mods` assuming default Uplay path.
A short Tutorial for Mod Creation with the Modloader is given below. For an example zoom extend mod see the `examples` directory. 


# Asset modding

In previous anno games there was a way to tell the game to load extacted files from disk instead of loading them  
from the RDA container. While that made it easier, it's still not a nice way to handle modding large XML files.

This Anno 1800 Mod loader supports a few simple 'commands' to easily patch the XML to achieve pretty much whatever you want.  

## How to Create a Patch for any XML File from the Game: 

Step 1) Set up a Directory for your Mod inside Anno 1800/mods. In the following steps, it is assumed that you have titled your Directory "myMod"

Step 2) inside of myMod, you recreate the exact File Structure that the base game uses. A patched assets.xml File would have to be under the following path: `Anno 1800/mods/myMod/data/config/export/main/asset/assets.xml`

Step 3) Your XML Document is expected to have the following structure: 
```xml
<ModOps>
    <ModOp>
        <!-- Whatever Change you want to do -->
    </ModOp>
</ModOps>
```
> You can give as many ```<ModOp>``` as you'd like to and have multiple patch files for different Original ones in a single Mod. 

## How to Write a ModOp
    
Step 1) Look up and Select the XML node you want to edit with XPath using the Path Argument. 

Example: 
```xml <ModOp Path = "/Templates/Group[Name = 'Objects']/Template[Name = 'Residence7']/Properties"> ```

For the assets File, you can also use the GUID Argument. This selects all the Child Nodes of the Asset with the given GUID as new Roots for your xPath for Cleaner Code and is also much faster, performance-wise. 

Example: 
```xml
    Standard Way:               <ModOp Path = "//Asset[Values/Standard/GUID = '1137']/Values/Standard/Name">
    
    Better, with GUID Arg:      <ModOp GUID = '1337' Path = "/Values/Standard/Name"> 
```
Step 2) Give a Type for a ModOp, to change the selected Node. 

Currently supported Types: 
```
- Merge                 Replaces all given child nodes
- Remove                Removes the selected Node
- Add                   Adds inside the selected Node
- Replace               Replaces the selected Node
- AddNextSibling        Adds a sibling directly after the selected node   
- AddPrevSibling        Adds a sibling directly in front of the selected node
```
> This was just a quick initial implementation (~3h), very open for discussions on how to make that better or do something entirely different

Step 3) Add the xml Code that you want to have added, merged or as replacement inside the ModOp. F
Example: 
```xml
    <ModOp Type = "replace" GUID = '1337' Path = "/Values/Standard/Name">
        <Name>ThisIsATestNameForGUID1337</Name>
    </ModOp>
```
> This ModOp will replace the Node under /Values/Standard/Name of the Asset with GUID 1337 with "```<Name>ThisIsATestNameForGUID1337</Name>```"

or a more practical example look up the tutorial for an additional zoom level below. 

# Debugging

Debugging will not be possible, the game is using Denuvo and VMProtect, I have my own tools that allow me to debug it, but I will not be sharing those publicly. 

> You can read a printf aka debug-log about any errors caused by missing Nodes, Wrong Paths or Unrecognized Node Tests in ```Anno 1800/logs/mod-loader.log``` 

To test what a 'patch' you write does to the Original Game File, you can also use `xml-test`, which will simulate what the game will load.

```
xml-test game_camera.xml patch.xml
```

> This patches game_camera.xml with patch.xml and writes the result as a patched.xml file in the current directory

Original whitespace should be pretty much the same, so you can use some diff tool to see exactly what changed.

## Tutorial: Adding a new zoom level

Put this in a mod folder with the game path
so this would be in `mods/new-zoom-level/data/config/game/camera.xml`

> The mods folder in a default Uplay installation has to be located at `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\mods`

```xml
<ModOp Type="add" Path="/Normal/Presets">
    <Preset ID="15" Height="140" Pitch="0.875" MinPitch="-0.375" MaxPitch="1.40" Fov="0.56" />
</ModOp>
<ModOp Type="merge" Path="/Normal/Settings">
    <Settings MaxZoomPreset="15"></Settings>
</ModOp>
```

You can find more examples in the `examples` directory.  


## Other files

Other file types can't be 'merged' obviously, so there we just load the version of the last mod that has that file. (Mods are loaded alphabetically).
For Resources it is heavily recommended to use the Anno 1800/data Folder.  

# Building

You need Bazel, Visual Studio 2019 and that _should_ be it.  
You can checkout `azure-pipelines.yml` and see how it's done there.

If you want to work on new features for XML operations, you can use xmltest for testing. As that is using the same code as the actualy file loader.

# Coming soon (maybe)

- Access to the Anno python api, the game has an internal python API, I am not yet at a point where I can say how much you can do with it, but I will be exploring that in the future.
