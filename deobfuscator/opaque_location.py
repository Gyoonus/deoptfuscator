#!/usr/bin/env python3
import subprocess
import os

profile = ''
stdout = ''
stderr = ''
def opaque_location(apk_info):
    
    print(apk_info)
    global stdout
    global stderr
    child = subprocess.Popen([os.getenv('ANDROID_HOST_OUT')+'/bin/OLocation'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    output = child.communicate(apk_info)
    print('output:   ' + str(output))
    child_stdout, child_stderr = output[0], output[1]
    if len(output[1]) :
        stderr.write(output[1])
        print("Error(opaque_location) : " +  apk_info)
        return False
    else :
        a = (output[0].split('\n'))
        for wr in a[4:-7] :
            stdout.write(wr + "\n")
        stdout.write("----------\n")
        return True
    
def opaque_locations(apk):
    
    profile = open("./.profile", 'r')
    global stdout
    global stderr
    stdout = open(os.environ['ROOT']+"/.stdout2", 'w')
    stderr = open(os.environ['ROOT']+"/.stderr2", 'w')
    lines = profile.readlines()
    print("lines:    ::::   " + str(lines))
    
    for line in lines : 
        if not opaque_location(apk + " " + line) :
            stdout.close()
            return False
    stdout.close()
    return True
