## What is Deoptfuscator

+ Deoptfuscator is a tool for deobfuscating Android applications that have been transformed using control-flow obfuscation mechanisms.
+ Deoptfuscator can reverse the control-flow obfuscation performed by DexGuard on open-source Android applications.

## Prerequisites
In order to build and run deoptfuscator, the followings are required:
+ Deoptfuscator based on Ubuntu 18.04 LTS 64bit PC
  + libboost, libjson (C++ library)
    + $sudo apt-get install libboost-all-dev
    + $sudo apt-get install libjsoncpp-dev
  + openjdk
    + $sudo apt-get install openjdk-11-jdk
  + i386 libc
    + $sudo dpkg --add-architecture i386
    + $sudo apt-get update
    + $sudo apt-get install libc6:i386 libstdc++6:i386
  + zipalign
    + $sudo apt-get install zipalign
  + apksigner
    + $sudo apt-get install apksigner


## HOW TO USE
+ Set Local Environment
  + $. ./launch.sh 
+ Deobfuscate an Android application that has been transformed using control-flow obfuscated techniques.
  + $python3 deoptfuscator.py <obfuscated_apk>
+ Test our tools!
  + $python3 deoptfuscator.py test/Trolly_cf.apk
  + If the input file (an obfuscated app) was "Trolly_cf.apk", the file name of the deobfuscated apk is [Trolly_cf_deobfuscated_align.apk]

+ Our tool can effectively deobfuscate Android applications transformed with the control flow obfuscation option of DexGuard :
  + Our tool can currently handle the control-flow obfuscation techniques of DexGuard.
  + It cannot handle other obfuscation techniques such as layout obfuscation, identifier renaming, and string encryption.
![obfuscation](/images/effect.png)
