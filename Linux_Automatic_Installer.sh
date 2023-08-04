#!/bin/bash
echo "Welcome to zero's anno 1800 Linux installer! "
echo "What is your anno 1800 directory? (default = ~/.local/share/Steam/steamapps/common/Anno 1800/)"

directory=""
read -p "Enter directory or default: " directory

if [ $directory = "default" ]; then

    directory="~/.local/share/Steam/steamapps/common/Anno 1800/"

else
     read -p "Is this correct? $directory
     Please enter yes or no:
     " yn
     case $yn in
     [Yy]* ) cd $directory ;;
     [Nn]* ) exit;;
     esac
fi

cd ~/$directory
mkdir mods

cd ~/$directory/Bin/Win64
wget https://github.com/xforce/anno1800-mod-loader/releases/download/v0.9.4/loader.zip
unzip -o loader.zip
rm loader.zip

