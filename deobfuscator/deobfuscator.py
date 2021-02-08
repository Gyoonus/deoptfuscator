from profile import profile
import opaque_id
import opaque_location
import os
import subprocess
from classes import dexfile
dex = "../.apk/classes.dex"

def main(dex):
    '''
    Entry Point of deobfuscator
    [dex] : APK's path
    '''
    ret = opaque_id.opaque_id(dex)

    if not ret :
        return

    profile(".stdout")
    ret = opaque_location.opaque_locations(dex)
    print("location : " + str(ret))  

    dexfile(dex)
