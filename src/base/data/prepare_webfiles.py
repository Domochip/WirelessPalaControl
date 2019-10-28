import sys
import os
import gzip
import shutil
import struct

#Convert one file to header
#file is first GZipped then convert to header file (hex in PROGMEM)
def convert_file_to_cppheader(filename):
    with open(filename,'rb') as webfile:
        with gzip.open(filename+'.gz','wb',9) as intogzipfile:
            shutil.copyfileobj(webfile,intogzipfile)
            intogzipfile.close()
            webfile.close()
    with open(filename+'.gz','rb') as gzfile:
        with open(filename+'.gz.h','w') as hfile:
            hfile.write('const PROGMEM char '+filename.replace(' ','').replace('.','').replace('-','')+'gz[] = {')
            byte=gzfile.read(1)
            first=True
            while len(byte):
                if not first:
                    hfile.write(',')
                if sys.version_info.major == 2:
                    hfile.write(hex(ord(byte)))
                else:
                    hfile.write(hex(byte[0]))
                first=False
                byte=gzfile.read(1)
            hfile.write('};')
            hfile.close()
            gzfile.close()
    os.remove(filename+'.gz')

#Convert all Web Files in a folder
def convert_all_webfiles(dir):
    curentDir=os.getcwd()
    os.chdir(dir)
    for file in os.listdir('.'):
        if file.endswith(('.html','.js','.css')):
            convert_file_to_cppheader(file)
    os.chdir(curentDir)

convert_all_webfiles(r'.\src\base\data')
convert_all_webfiles(r'.\src\data')