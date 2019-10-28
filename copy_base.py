import os
import shutil

sourceFolder = r'..\WirelessBase\src\base'
destinationFolder = r'.\src\base'

if not os.path.exists(r'.\src\WirelessBase.h') and os.path.exists(sourceFolder) and os.path.abspath(sourceFolder).lower() != os.path.abspath(destinationFolder).lower():
    shutil.rmtree(destinationFolder,True)
    shutil.copytree(sourceFolder,destinationFolder)