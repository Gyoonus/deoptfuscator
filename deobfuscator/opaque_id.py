import subprocess
import os

def opaque_id(apk):
    print(apk)
    child = subprocess.Popen([os.getenv('ANDROID_HOST_OUT')+'/bin/OTest'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    output = child.communicate(apk)
    child_stdout, child_stderr = output[0], output[1]
    stdout = open("./.stdout", 'w')
    stderr = open("./.stderr", 'w')
    if len(output[1]) :
        stderr.write(output[1])
        print("Error : " +  apk)
        return False
    else :
        stdout.write(output[0])
        return True
