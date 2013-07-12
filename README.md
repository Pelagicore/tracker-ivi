### Tracker-IVI
Tracker-IVI is an In-Vehicle Infotainment specific distribution of the Tracker
software (https://projects.gnome.org/tracker/). The purpose of this
distribution is to provide a standard configuration of Tracker suitable for
indexing multimedia in cars. 

Many of the features of the full Tracker project have been disabled to minimize
the installed size of the software, while some other features not neccessarily
interesting for normal desktop use, such as specialized extractors, have been
added.

#### Dependencies
The default configuration of Tracker-IVI has the following dependencies:

| Software      | Version       |
| ------------- | ------------- |
| DBus          | >= 1.3.1      |
| GLib          | >= 2.35.1     |
| GdkPixBuf     | >= 2.12.0     |
| libjpeg       | any version   |
| libgif        | any version   |
| libtiff       | any version   |
| libpng        | >= 0.89       |
| libvorbis     | >= 0.22       |
| libflac       | >= 1.2.1      |
| libexif       | >= 0.6        |
| GStreamer     | >= 1.0.1      |
| SQLite        | >= 3.7.9      |

#### Installation
With all dependencies installed, installation should be straight forward:

```bash
build-tracker-ivi
make install
```

build-tracker-ivi is a makefile which invokes autogen.sh with a specific set of `--enable` and `--disable` switches. The switches are easily configurable in the build-tracker-ivi makefile, and this is a good place to specify `--prefix`, etc. Once `autogen.sh` has been run, `make` is invoked automatically.
