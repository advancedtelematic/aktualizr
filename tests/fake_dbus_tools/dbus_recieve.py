#!/usr/bin/python3
import dbus.service
import dbus.glib
from gi.repository import GObject
import dbus
import sys
import os

temp_file = "/tmp/dbustestclient.txt"
try:
    os.remove(temp_file)
except:
    pass
loop = GObject.MainLoop()
bus = dbus.SessionBus()


def catchall_handler(*args, **kwargs):

    if kwargs['dbus_interface'] == "org.genivi.SotaClient":
        fl = open(temp_file, "w")
        print()
        print()
        print(kwargs['member'])
        print()
        print()
        if kwargs['member'] == "DownloadComplete":
            dc = args[0]
            if dc[0] == "testupdateid" and dc[1] == "/tmp/img.test" and dc[2] == "signature":
                fl.write("DownloadComplete")
        elif kwargs['member'] == "UpdateAvailable":
            dc = args[0]
            if dc[0] == "testupdateid" and dc[1] == "testdescription" and dc[2] == "signature" and dc[3] == 200:
                fl.write("UpdateAvailable")
        elif kwargs['member'] == "InstalledSoftwareNeeded":
                fl.write("InstalledSoftwareNeeded")
        fl.flush()
        fl.close()



bus.add_signal_receiver(catchall_handler,
                        interface_keyword='dbus_interface',
                        member_keyword='member')

loop.run()