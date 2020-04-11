from profile import profile
import opaque_id
import opaque_location
from classes import dexfile
dex = "../.apk/classes.dex"

def main(dex):
    #ret = opaque_id("/mnt/test/01_ControlFlow/01_High/sanity_cf.apk")
    ret = opaque_id.opaque_id(dex)
    if not ret :
        return
    profile(".stdout")
    ret = opaque_location.opaque_locations(dex)
    print("location : " + str(ret))  
    if not ret :
        return
    dexfile(dex)
#main(dex)
