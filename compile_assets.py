#!/usr/bin/env python

"""
This will process all the stuff in the assets dir and put it in the data dir
"""

import os
from shutil import copyfile
import platform
import subprocess
import sys
import time



# parse args
assert len(sys.argv) >= 4
src_dirname = sys.argv[1]
dst_dirname = sys.argv[2]
target_platform = sys.argv[3]



# we need blender and some export scripts
os_name = platform.system()
if os_name == "Darwin":
	blender_bin = "/Applications/Blender 2.74/blender.app/Contents/MacOS/blender"
	nvtextool_bin = "/usr/local/bin/nvcompress"
	pvrtextool_bin = "/Applications/Imagination/PowerVR_Graphics/PowerVR_Tools/PVRTexTool/CLI/OSX_x86/PVRTexToolCLI"
elif os_name == "Linux":
	blender_bin = "blender"
	nvtextool_bin = "nvcompress"
	pvrtextool_bin = "PVRTexToolCLI"
else:
	print("unsupported os")
	sys.exit(1)

export_model_script = "scripts/blender/export_model.py"
export_level_script = "scripts/blender/export_level.py"



# helper funcs
def makeDirIfNotExists(dirname):
	if not os.path.exists(dirname):
		os.makedirs(dirname)

def isFileNewer(src_filename, dst_filename):
	return os.path.getmtime(src_filename) > os.path.getmtime(dst_filename)

def setTGAOriginTop(filename):
	f = open(filename, "r+b")
	f.seek(17)
	f.write(b'\x20')
	f.close()



# make sure out dir exists
makeDirIfNotExists(dst_dirname)



# process 3D models (props and characters)
makeDirIfNotExists(dst_dirname+"/models")

for model in os.listdir(src_dirname+"/models"):
	if os.path.splitext(model)[1] != '.blend':
		continue

	src_model_filename = src_dirname+"/models/"+model
	dst_model_filename = dst_dirname+"/models/"+os.path.splitext(model)[0]+".mdl"

	do_compile_model = False
	if os.path.exists(dst_model_filename):
		do_compile_model = isFileNewer(src_model_filename, dst_model_filename)
	else:
		do_compile_model = True
	#do_compile_model = True # for debugging

	if do_compile_model:
		print("compiling "+src_model_filename)
		subprocess.call([blender_bin, "-b", src_model_filename, "-P", export_model_script, "--", "--out", dst_model_filename, "--use_16bit_indices"])



# process levels
if os.path.exists(src_dirname+"/levels"):
	makeDirIfNotExists(dst_dirname+"/levels")
	for level in os.listdir(src_dirname+"/levels"):
		if (os.path.isdir(src_dirname+"/levels/"+level)):
			src_level_filename = src_dirname+"/levels/"+level+"/"+level+".blend"
			dst_level_dir = dst_dirname+"/levels/"+level
			dst_level_filename = dst_level_dir+"/model.mdl"

			if not os.path.exists(src_level_filename):
				print("blend file for level "+level+" not found")
				continue

			do_compile_level = False
			if not os.path.exists(dst_level_dir):
				os.mkdir(dst_level_dir)
				do_compile_level = True
			else:
				if os.path.exists(dst_level_filename):
					do_compile_level = isFileNewer(src_level_filename, dst_level_filename)
				else:
					do_compile_level = True

			if do_compile_level:
				print("compiling "+src_level_filename)
				subprocess.call([blender_bin, "-b", src_level_filename, "-P", export_level_script, "--", "--out", dst_level_dir, "--use_16bit_indices"])

# always compile test level (testing export script)
#subprocess.call([blender_bin, "-b", src_dirname+"/levels/dust/dust.blend", "-P", export_level_script, "--", "--out", dst_dirname+"/levels/dust", "--use_16bit_indices"])

# compress textures
makeDirIfNotExists(dst_dirname+"/textures")

for texture in os.listdir(src_dirname+"/textures"):
	src_texture_filename = src_dirname+"/textures/"+texture
	if target_platform == "pandora":
		dst_texture_filename = dst_dirname+"/textures/"+os.path.splitext(texture)[0]+".pvr"
	else:
		dst_texture_filename = dst_dirname+"/textures/"+os.path.splitext(texture)[0]+".dds"

	if not os.path.exists(dst_texture_filename) or isFileNewer(src_texture_filename, dst_texture_filename):
		print("compressing "+texture)
		if target_platform == "pandora":
			subprocess.call([pvrtextool_bin, "-m", "-q", "pvrtcbest", "-f", "PVRTC1_4_RGB", "-i", src_texture_filename, "-o", dst_texture_filename])
		else:
			subprocess.call([nvtextool_bin, "-silent", src_texture_filename, dst_texture_filename])


# convert ui graphics into tga format using imagemagick
if os.path.exists(src_dirname+"/gfx"):
	makeDirIfNotExists(dst_dirname+"/gfx")

	for gfx in os.listdir(src_dirname+"/gfx"):
		src_gfx_filename = src_dirname+"/gfx/"+gfx
		dst_gfx_filename = dst_dirname+"/gfx/"+os.path.splitext(gfx)[0]+".tga"

		if not os.path.exists(dst_gfx_filename) or isFileNewer(src_gfx_filename, dst_gfx_filename):
			print("converting "+gfx)
			subprocess.call(["convert", "-strip", src_gfx_filename, "-flip", "-type", "truecolormatte", dst_gfx_filename])
			setTGAOriginTop(dst_gfx_filename)
			#subprocess.call(["sh", "scripts/set_tga_origin_top.sh", dst_gfx_filename])

# copy truetype fonts
fontlist = ["OpenSans/LICENSE.txt", "OpenSans/OpenSans-Regular.ttf"]
for font in fontlist:
	src_font_filename = src_dirname+"/fonts/"+font
	dst_font_filename = dst_dirname+"/fonts/"+font

	makeDirIfNotExists(os.path.dirname(dst_font_filename))

	if not os.path.exists(dst_font_filename) or isFileNewer(src_font_filename, dst_font_filename):
		print("copying "+src_font_filename+" to "+dst_font_filename)
		copyfile(src_font_filename, dst_font_filename)
