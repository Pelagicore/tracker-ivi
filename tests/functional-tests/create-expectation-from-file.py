#!/usr/bin/env python

import ConfigParser, sys, argparse, subprocess, StringIO, os.path, glob, time
from urllib import quote as urlquote
from pipes import quote

AVPROBE = "/usr/bin/avprobe -v 0 -show_streams -show_format -of ini %s"
EXIFTOOL = "exiftool -s %s | sed 's/^\(\w\+\)\s\+:\\s\+/\\1=/g' "+\
           "| sed '1s/^/[Props]\\n/'"
AUDIO_FMTS = [".mp3"]
VIDEO_FMTS = [".avi", ".mkv", ".rm", ".wmv", ".mp4"]
IMAGE_FMTS = [".jpg", ".jpeg", ".png", ".gif"]
class MediaTypes:
	AUDIO = 1
	VIDEO = 2
	IMAGE = 3

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
	elif genre == "Gangsta":
		return "Gangsta Rap"
	else: return genre

def avprobeDate(date):
	return time.strftime("%Y-%m-%dT%H:%M:%SZ",
	      time.strptime(date, "%Y-%m-%d %H\\:%M\\:%S"))

def exiftoolDate(date):
	return time.strftime("%Y-%m-%dT%H:%M:%S",
	      time.strptime(date, "%Y:%m:%d"))

def correctFlash(flash):
	if flash == "No Flash":
		return "False"
	elif flash == "Fired":
		return "True"

def getMediaType(fname):
	for ext in AUDIO_FMTS:
		IMAGE_FMTS = [".jpg", ".jpeg", ".png", ".gif"]
		if fname.lower().endswith(ext.lower()):
			return MediaTypes.AUDIO
	for ext in VIDEO_FMTS:
		if fname.lower().endswith(ext.lower()):
			return MediaTypes.VIDEO
	for ext in IMAGE_FMTS:
		if fname.lower().endswith(ext.lower()):
			return MediaTypes.IMAGE
	return None

def getFileMetadata(f, mediatype):
	if mediatype == MediaTypes.AUDIO or\
	   mediatype == MediaTypes.VIDEO:
		cmd = AVPROBE % quote(f)
	elif mediatype == MediaTypes.IMAGE:
		cmd = EXIFTOOL % quote(f)
	else:
		print "Bad file type"
		return
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
	print "Field %s is missing from section %s" % (field1, section1)
	return False

def setVideoProperties(cf1, cf2, cmp):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
	                         cf2, "Metadata", "ivi_videotitle",
	                         lambda x: x)
	success = cmp (success, exportProperty(cf1, "format.tags",
	                         "creation_time", cf2, "Metadata",
	                         "ivi_filecreated", avprobeDate))
	return success

def setAudioProperties(cf1, cf2, cmp):
	success = True
	filename = os.path.basename(cf1.get("format", "filename"))
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	success = exportProperty(cf1, "format.tags", "title",
		                 cf2, "Metadata", "ivi_trackname",
	                         lambda x: x)
	success = cmp (success, exportProperty(cf1, "format.tags", "artist",
	                         cf2, "Metadata", "ivi_trackartist",
	                         lambda x: "<urn:artist:%s>" % uriencode(x)))
	success = cmp (success, exportProperty(cf1, "format.tags", "album",
	                         cf2, "Metadata", "ivi_trackalbum",
	                         lambda x: "<urn:album:%s>" % uriencode(x)))
	success = cmp (success, exportProperty(cf1, "format.tags", "genre",
	                         cf2, "Metadata", "ivi_trackgenre",
	                         correctGenre))
	success = cmp (success, exportProperty(cf1, "format.tags", "track",
	                         cf2, "Metadata", "ivi_tracktracknumber",
	                         lambda x: x))
	success = cmp (success, exportProperty(cf1, "format.tags","date",
	                         cf2, "Metadata", "ivi_trackcreated",
	                         lambda x: "%s-01-01T00:00:00Z" % x))
	return success

def setImageProperties(cf1, cf2, cmp):
	success = True
	filename = cf1.get("Props", "FileName")
	cf2.add_section("TestFile")
	cf2.set("TestFile", "Filename", filename)
	extension = filename.split(".")
	if len(extension) > 1:
		extension = extension[-1].lower()
	success = exportProperty(cf1, "Props", "Title",
	                         cf2, "Metadata", "ivi_imagetitle",
	                         lambda x: x)
	if extension is "png":
		success = cmp (success, exportProperty(cf1, "Props",
		   "CreationTime", cf2, "Metadata", "ivi_filecreated",
		   exiftoolDate))
	if extension is "jpg":
		success = cmp (success, exportProperty(cf1, "Props",
		   "DateTimeOriginal", cf2, "Metadata", "ivi_filecreated",
		   exiftoolDate))
	else: # If we don't know of this extension, try the Date tag
		success = cmp (success, exportProperty(cf1, "Props", "Date",
	                         cf2, "Metadata", "ivi_filecreated",
	                         exiftoolDate))
	success = cmp (success, exportProperty(cf1, "Props", "ImageWidth",
	                         cf2, "Metadata", "ivi_imagewidth",
	                         lambda x:x))
	success = cmp (success, exportProperty(cf1, "Props", "ImageHeight",
	                         cf2, "Metadata", "ivi_imageheight",
	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "Make",
#	                         cf2, "Metadata", "ivi_cameramanufacturer",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "Model",
#	                         cf2, "Metadata", "ivi_cameramodel",
#	                         lambda x:x))
	# Image orientation ?
#	success = cmp (success, exportProperty(cf1, "Props", "Copyright",
#	                         cf2, "Metadata", "ivi_imagecopyright",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "FNumber",
#	                         cf2, "Metadata", "ivi_imagefnumber",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "Flash",
#	                         cf2, "Metadata", "ivi_imageflash",
#	                         correctFlash))
#	success = cmp (success, exportProperty(cf1, "Props", "FocalLength",
#	                         cf2, "Metadata", "ivi_imagefocallength",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "ExposureTime",
#	                         cf2, "Metadata", "ivi_imageexposuretime",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "ISO",
#	                         cf2, "Metadata", "ivi_imageisospeed",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "Description",
#	                         cf2, "Metadata", "ivi_imagedescription",
#	                         lambda x:x))
	# Image date ?
	# Metering mode?

	success = cmp (success, exportProperty(cf1, "Props", "Author",
	                         cf2, "Metadata", "ivi_imageartist",
	                         lambda x: "<urn:artist:%s>" % uriencode(x)))
#	success = cmp (success, exportProperty(cf1, "Props", "Comment",
#	                         cf2, "Metadata", "ivi_imagecomment",
#	                         lambda x:x))
	# Address ?
#	success = cmp (success, exportProperty(cf1, "Props", "State",
#	                         cf2, "Metadata", "ivi_imagestate",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "City",
#	                         cf2, "Metadata", "ivi_imagecity",
#	                         lambda x:x))
#	success = cmp (success, exportProperty(cf1, "Props", "Country",
#	                         cf2, "Metadata", "ivi_imagecountry",
#	                         lambda x:x))
	# GPS coordinates ?
	# Artist vs Creator?

	return success

def getExpectFile(fn):
	fnn = fn.split(".")
	if len(fn) > 1:
		return '.'.join(fnn[:-1]) + ".expected"
	else:
		return fn + ".expected"

def processFile(fn):
	ftype = getMediaType(fn)
	if not ftype:
		print "Unrecognized file type: %s" % fn
		return

	cf1 = getFileMetadata(fn, ftype)
	cf2 = ConfigParser.RawConfigParser()
	cf2.optionxform = str

	cmp = lambda x,y: x or y
	if args.picky:
		cmp = lambda x,y: x and y

	if ftype == MediaTypes.AUDIO:
		if not setAudioProperties(cf1, cf2, cmp):
			print "Failed to set audio properties for %s" % fn
			return
	elif ftype == MediaTypes.VIDEO:
		if not setVideoProperties(cf1, cf2, cmp):
			print "Failed to set video properties for %s" % fn
			return
	elif ftype == MediaTypes.IMAGE:
		if not setImageProperties(cf1, cf2, cmp):
			print "Failed to set image properties for %s" % fn
			return
	ef = getExpectFile(fn)
	cf2.write(open(ef,"w"))
	print "Writing to: %s" % ef

parser = argparse.ArgumentParser(
	description="Generate .expect files for media files")

parser.add_argument("-f", "--file",type=str)
parser.add_argument("-d", "--dir", type=str)
parser.add_argument("-p", "--picky", action='store_true', default=False)

args = parser.parse_args()

if not (args.file or args.dir):
	parser.error("Supply a file or directory")

if __name__ == "__main__":
	if args.file:
		processFile(args.file)
	if args.dir:
		to_process = []
		for dirpath, dirnames, files in os.walk(args.dir):
			  dirnames[:] = filter (lambda x: not x.startswith("."),
					     dirnames)
			  for f in filter(lambda x: not x.endswith('.expected'), files):
				to_process.append(os.path.join(dirpath, f))
		for f in to_process:
			processFile(f)
