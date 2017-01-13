# Copyright (C) 2013 - 2015 BMW Group
# Author: Manfred Bathelt (manfred.bathelt@bmw.de)
# Author: Juergen Gehring (juergen.gehring@bmw.de)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import sys
import traceback
import gobject
import math
import dbus
import dbus.service

BASE_PATH = 'fake.legacy.service'
OBJECT_PATH = '/some/legacy/path/6259504'
    
def finish(interface):
    try:
        bus = dbus.SessionBus()
        remote_object = bus.get_object(BASE_PATH + '.connection', OBJECT_PATH)
        iface = dbus.Interface(remote_object, interface)
        iface.finish()
        return 0
    except:
        print "Service not existing, therefore could not be stopped"
        return 1

def main():
    command=sys.argv[1]
    interface=sys.argv[2]
    if command=="finish":
        return finish(interface)
    
    return 0

sys.exit(main())
