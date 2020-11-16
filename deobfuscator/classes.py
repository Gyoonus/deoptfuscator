import os
def dexfile(dex):
    df = open(dex, "rb")
    f = open(".stdout2", "r")
    print (dex)
    new_dexf = open (dex.replace("classes", "const/const"), "wb")
    print("new_dexf: " + str(new_dexf))
    dexf = df.read()
    classes = f.read().split("----------")
    print("read: " + str(classes))
    dexf_a = bytearray(dexf)
    for locations in classes:
        
        class_dic = {}
        location = locations.split("\n")
        lc = list(filter(('').__ne__, location))
        print("lc: " + str(lc))
        dex_locations = []
        for lcc in lc :
            if lcc.find(":") >= 0 :
                num = lcc.split(" : ")
                class_dic[int(num[0])] = int(num[1])
                print(str(class_dic))
            else :
                dex_locations.append(int(lcc, 16))
        for dex_location in dex_locations :
            
            print('dexf_a[dex_location]= ' + hex(dexf_a[dex_location]))
            f_idx = dexf_a[dex_location+2]
            print('f_dix = ' + hex(f_idx))
            add = dexf_a[dex_location+3] << 8
            print('add = ' + hex(add))
            print('b= ' + hex(dex_location), hex(f_idx), hex(add), hex(f_idx+add))
            dexf_a[dex_location] = 0x13
            print('dexf_a[dex_location = ' + hex(dexf_a[dex_location]))
            dexf_a[dex_location+2] = class_dic[f_idx+add]
            print('class_dic[f_idx+add] = ' + hex(class_dic[f_idx+add]))
            print('dexf_a[dex_location+2] = ' + hex(dexf_a[dex_location+2]))
    new_dexf.write(bytes(dexf_a))

    '''
    dexf_a = bytearray(dexf)
    for location in locations :
        a = (int(location.replace("\n", ""), 16))
        
        #dexf_a[a] = 0x13
        print(dexf_a[a],dexf_a[a+1],dexf_a[a+2],dexf_a[a+3])
    
    dexf = bytes(dexf_a)
    new_dexf = open ("./classes.dex", "wb")
    new_dexf.write(bytes(dexf_a))
    '''
#dexfile("/mnt/test/01_ControlFlow/01_High/test/classes.dex")
