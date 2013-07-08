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
| libvorbis     | >= 0.22       |
| libflac       | >= 1.2.1      |
| libexif       | >= 0.6        |
| GStreamer     | >= 1.0.1      |
| SQLite        | >= 3.7.9      |

#### Installation
With all dependencies installed, installation should be straight forward:

```bash
./autogen.sh --prefix=~/tracker_install
make
make install
```

The `autogen.sh` script specifies a series of configuration arguments to the
`configure` script, this behavior can be disabled using
`--disable-tracker-ivi`. It is not possible to specify `--enable` or
`--disable` switches for the `autogen.sh` script with the Tracker-IVI
configuration enabled.
