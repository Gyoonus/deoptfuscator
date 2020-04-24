## What is Deoptfuscator

+ Deoptfuscator is a tool for deobfuscating Android applications that have been transformed using control-flow obfuscation mechanisms.
+ Deoptfuscator can reverse the control-flow obfuscation performed by DexGuard on open-source Android applications.

## Prerequisites
In order to build and run deoptfuscator, the followings are required:
+ Deoptfuscator based on Ubuntu 18.04 LTS 64bit PC
  + libboost, libjson (C++ library)
    ```
    $ sudo apt-get install libboost-all-dev  
    $ sudo apt-get install libjsoncpp-dev
    ```
  + openjdk
    ```
    $ sudo apt-get install openjdk-11-jdk
    ```
  + i386 libc
    ```
    $ sudo dpkg --add-architecture i386  
    $ sudo apt-get update  
    $ sudo apt-get install libc6:i386 libstdc++6:i386
    ```
  + zipalign
    ```
    $ sudo apt-get install zipalign
    ```
  + apksigner
    ```
    $ sudo apt-get install apksigner
    ```
Our repository Already Contains Tools needed to run our Tool
 + Apktools : [https://ibotpeaches.github.io/Apktool/](https://ibotpeaches.github.io/Apktool/)
 + fbredex : [https://fbredex.com/](https://fbredex.com/)


## HOW TO USE
+ Set Local Environment  
  ``` 
  $. ./launch.sh
  ```
+ Deobfuscate an Android application that has been transformed using control-flow obfuscated techniques.  
  ```
  $ python3 deoptfuscator.py <obfuscated_apk>  
  ```
+ Test our tools!  
  ```
  $ python3 deoptfuscator.py sample_apk/AndroZoo_DexGuard_apk/apnapak.balance_transfer_89D2FCA810EEECA060549450E18A9D28.apk
  ```
  + If the input file (an obfuscated app) was `apnapak.balance_transfer_89D2FCA810EEECA060549450E18A9D28.apk`, the file name of the deobfuscated apk is `apnapak.balance_transfer_89D2FCA810EEECA060549450E18A9D28_deobfuscated_align.apk`

+ Our tool can effectively deobfuscate Android applications transformed with the control flow obfuscation option of DexGuard :
  + Our tool can currently handle the control-flow obfuscation techniques of DexGuard.
  + It cannot handle other obfuscation techniques such as layout obfuscation, identifier renaming, and string encryption.
![obfuscation](https://user-images.githubusercontent.com/64211521/80127450-4de81000-85cf-11ea-84fa-aeee68efab67.png)

## Acknowledgement

+ This research was supported by Basic Science Research Program through the National Research Foundation of Korea(NRF) funded by the Ministry of Science and ICT (no. 2018R1A2B2004830)
![시그니처 가로형_영문조합형](https://user-images.githubusercontent.com/64211521/80204259-7e798980-8663-11ea-95f1-ff19ccb86a77.jpg)

