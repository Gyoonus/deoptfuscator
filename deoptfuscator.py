 #!/usr/bin/env python3
import sys
import os
sys.path.insert(0, 'deobfuscator')
import deobfuscator
import sys
#from os.path import getsize

if len(sys.argv) != 2:
    print("Insufficient arguments")
    sys.exit()
apk_name = sys.argv[1]
outpath = "de"+sys.argv[1]
tmp = outpath.split("/")[-1]
outpath = outpath.replace(tmp, "")
os.system("rm -rf .apk .std* .profile meta")
os.system("java -jar $TOOLS/apktool.jar d -r -s " + apk_name  + " -o .apk")
dex_li = [a for a in os.listdir(".apk") if a.endswith(".dex") and a.startswith("classes")]
os.mkdir(".apk/const")
#os.makedirs(outpath, exist_ok=True)

for dex in dex_li:
	deobfuscator.main(".apk/"+dex)
	os.system("$TOOLS/redex-all -c $TOOLS/default.config .apk/const/const.dex -o .apk/const")
	print("$TOOLS/redex-all .apk/const/const.dex -o .apk/const")
	os.system("mv .apk/const/classes.dex .apk/"+dex)

apk_name = apk_name.replace(".apk", "_deobfuscated.apk")
apk_name = os.path.basename(apk_name)
os.system("java -jar $TOOLS/apktool.jar b ./.apk -o " + apk_name)

os.system("zipalign -f -v 4 " + apk_name + " " + apk_name.replace(".apk", "_align.apk"))
os.system("apksigner sign --ks deoptfuscator.keystore --ks-pass pass:123456 " + apk_name.replace(".apk", "_align.apk"))
os.system("rm -rf " + apk_name)
