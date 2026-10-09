# Brasero

Brasero is a CD/DVD mastering tool for the GNOME Desktop. It is designed to be simple and easy to use.

## Features

### Data CD/DVD

- Supports editing disc contents (remove, move, and rename files inside directories)
- Can burn data CD/DVD on the fly
- Automatic filtering for unwanted files (hidden files, broken/recursive symlinks, files not conforming to Joliet standard, ...)
- Supports multisession
- Supports Joliet extension
- Can write the image to the hard drive

### Audio CD

- Writes CD-TEXT information (automatically found thanks to GStreamer)
- Supports editing CD-TEXT information
- Can burn audio CD on the fly
- Can use all audio files handled by local GStreamer installation (`ogg`, `flac`, `mp3`, ...)
- Can search for audio files inside dropped folders
- Can insert a pause
- Can split a track

### CD/DVD Copy

- Can copy a CD/DVD to the hard drive
- Can copy DVD and CD on the fly
- Supports single-session data DVD
- Supports any kind of CD
- Can copy encrypted Video DVDs (needs `libdvdcss`)

### Other Features

- Erase CD/DVD
- Can save and load projects
- Can burn CD/DVD images and CUE files
- Song, image, and video previewer
- Device detection thanks to HAL
- File change notification (requires kernel > 2.6.13)
- Supports Drag and Drop / Cut and Paste from Nautilus (and other apps)
- Can use files on a network as long as the protocol is handled by `gnome-vfs`
- Can search for files thanks to Beagle (search is based on keywords or on file type)
- Can display a playlist and its contents (note that playlists are automatically searched through Beagle)
- All disc I/O is done asynchronously to prevent the application from blocking
- Default backend is provided by `cdrtools`/`cdrkit`, but `libburn` can be used as an alternative

## Notes on Plugins for Advanced Users

### Configuration

From the UI you can only configure (choose to use or not to use mostly) non-essential plugins; that is all those that don't burn, blank, or image.

If you really want to choose which of the latter you want Brasero to use, one simple solution is to remove the offending plugin from the Brasero plugin directory (`<install_path>/lib/brasero/plugins/`) if you're sure that you won't want to use it.

You can also set priorities between plugins. They all have a hardcoded priority that can be overridden through GSettings:

- If you set this key to `-1`, this turns off the plugin.
- If you set this key to `0`, this leaves the internal hardcoded priority — the default that basically lets Brasero decide what's best.
- If you set this key to more than `0`, then that priority will become the one of the plugin — the higher the value, the more chance it has to be picked up.

### Additional Notes

Some plugins have overlapping functionalities (e.g. `libburn`/`wodim`/`cdrecord`/`growisofs`, `mkisofs`/`libisofs`/`genisoimage`), but they don't always do the same things or sometimes they don't do it in the same way. Some plugins have a "specialty" where they are the best. That's why it's usually good to have them all around.

As examples:

- `growisofs` is good at handling DVD+RW and DVD-RW restricted overwrite
- `cdrdao` is best for on-the-fly CD copying
- `libburn` returns progress when it blanks/formats

## Requirements

- `gtk+` >= 3.x
- `gnome` 3.x (`gio`)
- `gstreamer` (>= 1.0.0)
- `libxml2`
- `cdrtools` or `cdrkit`
- `growisofs`
- A fairly new kernel (>= 2.6.13 because of `inotify`) (optional)
- `cairo`
- `libcanberra`
- `totem` (>= 3.0) (optional)
- `tracker` (>= 0.10.0) (optional)
- `libburn` (>= 0.4.0) (optional)
- `libisofs` (>= 0.6.2) (optional)

## Use of Generative AI

This project does not allow contributions generated entirely by large languages models (LLMs) and chatbots. This ban includes, but is not limited to, tools like ChatGPT, Claude, Copilot, DeepSeek, and Devin AI. We are taking these steps as precaution due to the potential negative influence of AI generated content on quality, as well as likely copyright violations.

This ban of AI generated content applies to all parts of the projects, including, but not limited to, code, documentation, issues, and artworks. An exception applies for purely translating texts for issues and comments to English.

AI tools can be used to answer questions and find information. However, we encourage contributors to avoid them in favor of using [existing documentation](https://developer.gnome.org) and our [chats and forums](https://welcome.gnome.org). Since AI generated information is frequently misleading or false, we cannot supply support on anything referencing AI output.
