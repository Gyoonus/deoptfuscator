import os
def dexfile(dex):
    df = open(dex, "rb")
    f = open(".stdout2", "r")
    print (dex)
    new_dexf = open (dex.replace("classes", "const/const"), "wb")
    dexf = df.read()
    classes = f.read().split("----------")
    dexf_a = bytearray(dexf)
    for locations in classes:
        
        class_dic = {}
        location = locations.split("\n")
        lc = list(filter(('').__ne__, location))
        dex_locations = []
        for lcc in lc :
            
            if lcc.find(":") >= 0 :
                num = lcc.split(" : ")
                class_dic[int(num[0])] = int(num[1])
            
            else if lcc.find("No") >= 0 :
                break
                
            else :
                dex_locations.append(int(lcc, 16))
        
        if bool(class_dic) : 
            for dex_location in dex_locations :
                f_idx = dexf_a[dex_location+2]
                add = dexf_a[dex_location+3] << 8
                dexf_a[dex_location] = 0x13
                dexf_a[dex_location+2] = class_dic[f_idx+add]
            
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
