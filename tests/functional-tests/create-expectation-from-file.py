#!/usr/bin/env python

import ConfigParser, sys, argparse, subprocess, StringIO, os.path, glob
from urllib import quote as urlquote
from pipes import quote

AVPROBE = "/usr/bin/avprobe"
AUDIO_FMTS = [".mp3"]
VIDEO_FMTS = [".avi", ".mkv", ".rm", ".wmv"]
class MediaTypes:
	AUDIO = 1
	VIDEO = 2

def uriencode(s):
	s = urlquote(s, ",'")
#	s = s.replace(",","%2C")
	return s

def correctGenre(genre):
	""" Tracker does not follow the ID3 standard regarding genre names """
	if genre == "Psychadelic":
		return "Psychedelic"
	elif genre == "AlternRock":
		return "Alt. Rock"
	else: return genre

def getMediaType(fname):
	for ext in AUDIO_FMTS:
		if fname.endswith(ext):
			return MediaTypes.AUDIO
	for ext in VIDEO_FMTS:
		if fname.endswith(ext):
			return MediaTypes.VIDEO
	return None

def vals():
	configParser = ConfigParser.RawConfigParser ()
	# Make it case sensitive:
	configParser.optionxform = str
	configParser.add_section("TestFile")
	configParser.set("TestFile", "Filename", "Some path")
	configParser.write(sys.stdout)

def getFileMetadata(f):
	cmd = "%s -v 0 -show_streams -show_format -of ini %s" %\
	  (AVPROBE, quote(f))
	confParser = ConfigParser.RawConfigParser()
	confParser.optionxform = str

	out = subprocess.check_output(cmd, shell=True)
	outf = StringIO.StringIO(out)
	confParser.readfp(outf)
	return confParser

def exportProperty(parser1, section1, field1,
                   parser2, section2, field2,
                   formatter):
	if parser1.has_option(section1, field1):
		value = parser1.get(section1, field1)
		if not parser2.has_section(section2):
			parser2.add_section(section2)
		parser2.set(section2, field2, formatter(value))
		return True
	print "Field %s is missing from section %s" % (section1, field1)
	return False

def setVideoProperties(cf1, cf2):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
	                         cf2, "Metadata", "ivi_videotitle",
	                         lambda x: x)
	return success

def setAudioProperties(cf1, cf2):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
		                 cf2, "Metadata", "ivi_trackname",
	                         lambda x: x)
	success = success and exportProperty(cf1, "format.tags", "artist",
	                         cf2, "Metadata", "ivi_trackartist",
	                         lambda x: "<urn:artist:%s>" % uriencode(x))
	success = success and exportProperty(cf1, "format.tags", "album",
	                         cf2, "Metadata", "ivi_trackalbum",
	                         lambda x: "<urn:album:%s>" % uriencode(x))
	success = success and exportProperty(cf1, "format.tags", "genre",
	                         cf2, "Metadata", "ivi_trackgenre",
	                         correctGenre)
	success = success and exportProperty(cf1, "format.tags", "track",
	                         cf2, "Metadata", "ivi_tracktracknumber",
	                         lambda x: x)
	success = success and exportProperty(cf1, "format.tags","date",
	                         cf2, "Metadata", "ivi_trackcreated",
	                         lambda x: "%s-01-01T00:00:00Z" % x)
	return success

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
	ftype = getMediaType(fn)
	if not ftype:
		print "Unrecognized file type: %s" % fn
		return
	elif ftype == MediaTypes.AUDIO:
		if not setAudioProperties(cf1, cf2):
			print "Failed to set audio properties for %s" %\
			      fn
			return
	elif ftype == MediaTypes.VIDEO:
		if not setVideoProperties(cf1, cf2):
			print "Failed to set video properties for %s" %\
			      fn
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
