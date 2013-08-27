#!/usr/bin/env python

import ConfigParser, sys, argparse, subprocess, StringIO, os.path, glob

AVPROBE = "/home/jonatan/libav/avprobe"
AUDIO_FMTS = [".mp3"]
VIDEO_FMTS = [".avi", ".mkv", ".rm", ".wmv"]

def vals():
	configParser = ConfigParser.RawConfigParser ()
	# Make it case sensitive:
	configParser.optionxform = str
	configParser.add_section("TestFile")
	configParser.set("TestFile", "Filename", "Some path")
	configParser.write(sys.stdout)

def getFileMetadata(f):
	cmd = "%s -v 0 -show_streams -show_format -of ini %s" %\
	  (AVPROBE, f)
	confParser = ConfigParser.RawConfigParser()
	confParser.optionxform = str

	out = subprocess.check_output(cmd, shell=True)
	outf = StringIO.StringIO(out)
	confParser.readfp(outf)
	return confParser

def exportProperty(parser1, section1, field1,
                   parser2, section2, field2,):
	if parser1.has_option(section1, field1):
		value = parser1.get(section1, field1)
		if not parser2.has_section(section2):
			parser2.add_section(section2)
		parser2.set(section2, field2, value)
		return True
	return False

def setVideoProperties(cf1, cf2):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
		                 cf2, "Metadata", "ivi_videotitle")
	return success

def setAudioProperties(cf1, cf2):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
		                 cf2, "Metadata", "ivi_tracktitle")
	success = success and exportProperty(cf1, "format.tags", "artist",
		                 cf2, "Metadata", "ivi_trackartist")

def getExpectFile(fn):
	fnn = fn.split(".")
	if len(fn) > 1:
		return '.'.join(fnn[:-1]) + ".expected"
	else:
		return fn + ".expected"

def processFile(fn):
	cf1 = getFileMetadata(fn)
	cf2 = ConfigParser.RawConfigParser()
	cf2.optionxform = str
	if not setVideoProperties(cf1, cf2):
		return
	ef = getExpectFile(fn)
	cf2.write(open(ef,"w"))
	print "Writing to: %s" % ef

parser = argparse.ArgumentParser(
	description="Generate .expect files for media files")

parser.add_argument("-f", "--file",type=str)
parser.add_argument("-d", "--dir", type=str)

args = parser.parse_args()

if not (args.file or args.dir):
	parser.error("Supply a file or directory")

if __name__ == "__main__":
	if args.file:
		processFile(args.file)
	if args.dir:
		files = [os.path.join(dirpath, f)
	                  for dirpath, dirnames, files in os.walk(args.dir)
	                  for f in files if not f.endswith('.expected')]
		for f in files:
			processFile(f)
