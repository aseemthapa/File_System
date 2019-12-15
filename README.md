# File_System

This code simulates a file system.
The file system that can be created is nearly equal to 33 MB.

Commands that can be used:
1) createfs [disk_img_name] : creates a disk image of user defined name.
2) open [disk_img_name] : opens an already existing disk image.
NOTE: The following commands need creation or opening of a disk image before executing.
If executed withou an open disk image, these functions will return an error message
saying no disk image is open.
3) put [filename] : Puts something from host disk to the created disk.
4) get:
  a) get [filename] : gets a file with given filename and puts it into host disk.
  b) get [filename] [newFilename] : gets a file with given filename and puts it into host disk under the name <newFilename>.
5) del [Filename] : deletes a file from the disk_image.
6) list:
  a) list: lists files in the system.
  b) list -h: lists all files in the system includeing hidden ones.
7) df: returns the free space left.
8) attrib [attribute_change] [filename] : Changes the attributes of a file in the system
9) close [filename] : closes an already open disk image and saves it.

***NOTE : In order to save everything to a disk image/file system, one must execute the close command.
