#!/usr/bin/env python3
import os

def main():
    wmm_table=[]
    if not os.path.exists("WMM.COF"):
        print("Please copy latest WMM.COF from https://www.ngdc.noaa.gov/geomag/WMM/soft.shtml here.")
        exit(0)
    with open("WMM.COF","r") as rf:
        header=rf.readline()
        epoch=header[4:4+6]
        x = 1
        while True:
            line=rf.readline().replace("\n","")
            if line=="":
                break
            if "999999999999999999999999" in line:
                break
            data=line.split(" ")
            datatbl=[]
            for item in data:
                if item!="":
                    datatbl.append(item)
            wmm_table.append(datatbl)

        c0={}
        cd0={}
        for x in range(0,13):
            c0[x]={}
            cd0[x]={}
            for y in range(0,13):
                c0[x][y]="0.0"
                cd0[x][y]="0.0"

        index=1
        x=0
        for item in wmm_table:
            if x==0:
                c0[x][index]=item[2]
                cd0[x][index]=item[4]
                x+=1
            else:
                c0[x][index]=item[2]
                c0[index][x-1]=item[3]
                cd0[x][index]=item[4]
                cd0[index][x-1]=item[5]
                x+=1
            if index<x:
                x=0
                index+=1

    with open("wmm.h","w") as wf:
        wf.write("static double epoc = "+epoch+";\n")
        c0str=("const double c0[13][13]={\n")
        for x in range(0,13):
            c0str += "{\t"
            for y in range(0,13):
                c0str+=c0[x][y]+",\t"
            c0str+="},\n"
        c0str+="};\n"
        wf.write(c0str)

        cd0str = ("const double cd0[13][13]={\n")
        for x in range(0, 13):
            cd0str += "{\t"
            for y in range(0, 13):
                cd0str += cd0[x][y] + ",\t"
            cd0str += "},\n"
        cd0str += "};"
        wf.write(cd0str)
    print("Done converting.")

if __name__=="__main__":
    main()