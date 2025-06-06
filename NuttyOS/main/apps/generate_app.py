#!/usr/bin/env python3

import sys
import os
from distutils.dir_util import copy_tree

if len(sys.argv) != 3:
	print(sys.argv[0], "func_name", "app-name")
	exit()

func_name = sys.argv[1]
app_name = sys.argv[2]

print("Creating app", func_name, "(" + app_name + ")")

if os.path.isdir('./' + func_name):
	print("./" + func_name + " already exists.")
	exit()

target_dir = "./" + func_name

copy_tree("./_app_template", target_dir)
os.rename(target_dir + "/" + "_AppName.c", target_dir + "/" + func_name + ".c")
os.rename(target_dir + "/" + "_AppName.h", target_dir + "/" + func_name + ".h")

files = [func_name + ".c", func_name + ".h"]
for file in files:
	content = ""
	with open(target_dir + "/" + file, "r") as f:
		content = f.read()
	content = content.replace("_AppName", func_name)
	content = content.replace("APPNAME", func_name.upper())
	content = content.replace("_App Name", app_name)
	with open(target_dir + "/" + file, "w") as f:
		f.write(content)
