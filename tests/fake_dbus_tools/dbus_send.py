# -*- coding: utf-8 -*-
import sys
import dbus

name = "org.genivi.SotaClient"
bus = dbus.SessionBus()
obj = bus.get_object(name, "/org/genivi/SotaClient")
sota = dbus.Interface(obj, name)

#Wremote_method = obj.get_dbus_method("initiateDownload", name)
reports = {'id': dbus.String("123", variant_level=1),
        'result_code': dbus.Int32(0, variant_level=1),
        'result_text': dbus.String("Good", variant_level=1)}
        
if sys.argv[1] == "initiateDownload":
    sota.initiateDownload(dbus.String("testupdateid"))
elif sys.argv[1] == "abortDownload":
    sota.abortDownload(dbus.String("testupdateid"))
elif sys.argv[1] == "updateReport":
    sota.updateReport(dbus.String("testupdateid"), [reports])

