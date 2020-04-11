#!/usr/bin/env python3

import json
import os

def profile(path):
  with open(path,"r") as json_file:
    json_line = json_file.readlines()
    json_line = json_line[4:-6]
  with open('.profile2', "w") as json_file:
    for a in json_line :
      json_file.write(a)

  with open('.profile2',"r") as json_file:
    json_data = json.load(json_file)
  class_dic = {}
  profile_file = open(".profile", "w")
    


#class level
  for class_ in json_data:
    insns = 0;
    class_dic["id"] = class_.get("class")
    methods = {}
    class_dic["methods"] = methods
    if class_ == None:
      continue
    if class_.get("methods") == None:
      continue
    for method in class_.get("methods"):
      method_dic = {}
      f_dic={}

      if method.get("fields") == None:
        continue
      if len(method.get("fields")) == 0:
        continue
      method_dic["f_dic"] = f_dic
      method_dic["insns"] = method.get("insns")
      insns+= method.get("insns")
      method_dic["idx"] = method.get("idx")
      method_dic["id"] = method.get("id")
      methods[method_dic["idx"]] = method_dic
      for fields in method.get("fields"):
        for field in fields :
          if field == "if" :
            if not fields[field] in f_dic :
              f_dic[fields[field]] = {"if" :  1}
            else :
              if not "if" in f_dic[fields[field]]:
                f_dic[fields[field]]["if"] = 1
              else :
                f_dic[fields[field]]["if"] += 1
          
          elif field == "sget" :
            if len(fields[field]) == 2:
              sget = fields[field]
              if not sget[0] in f_dic :
                f_dic[sget[0]] = { sget[1] : 1}
              else : 
                if not sget[1] in f_dic[sget[0]]:
                  f_dic[sget[0]][sget[1]] = 1
                else :
                  f_dic[sget[0]][sget[1]] += 1

              if not sget[1] in f_dic :
                f_dic[sget[1]] = { sget[0] : 1}
              else :
                if not sget[0] in f_dic[sget[1]]:
                  f_dic[sget[1]][sget[0]] = 1
                else :
                  f_dic[sget[1]][sget[0]] += 1
      for f in f_dic:
        if not "if" in f_dic[f]:
          f_dic[f]["if"] = 0
        for ff in f_dic[f]:
          if ff == "if" :
            continue
    
    if len(class_dic["methods"]) < 1 : #method count option
      continue
    class_field = {}
    for f in class_dic["methods"] :
      for ff in class_dic["methods"][f]["f_dic"] :
        f_dic_ = class_dic["methods"][f]["f_dic"]
        #print (ff, f_dic_[ff])
        if not ff in class_field:
          class_field[ff] = f_dic_[ff]
        else:
          for fff in f_dic_[ff]:
            if not fff in class_field[ff]:
              class_field[ff][fff] =  f_dic_[ff][fff]
            else:
              class_field[ff][fff] += f_dic_[ff][fff]
    for f in class_field:
      for ff in class_field[f]:
        if ff == f:
          continue
        if not ff == "if":
          class_field[f][ff] += (class_field[f]["if"] + class_field[ff]["if"])
    for f in class_field:  
      del(class_field[f]['if'])
    
    mmax = 0
    a = []
    for f in class_field:
      inverse = [(value, key) for key, value in class_field[f].items()]
      if len(inverse) == 0:
        continue
      if mmax < max(inverse)[0]:
        a = [f,max(inverse)[1]]
        mmax = max(inverse)[0]
    if mmax/(insns/4) > 0.01:
      if a[0] != a[1] :
         #profile_file.write(class_dic["id"] + " " +  str(a[0]) + " " + str(a[1]) + " " + str(mmax/(insns/4))+"\n")
         profile_file.write(class_dic["id"] + " " +  str(a[0]) + " " + str(a[1]) + "\n")
         print(class_dic["id"] + " " +  str(a[0]) + " " + str(a[1]) + " " + str(mmax/(insns/4))+"\n")

