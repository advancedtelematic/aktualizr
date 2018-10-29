Aktualizr is the codename of the HERE OTA Connect client. This client runs on any embedded device and can check for updates periodically or triggered by another system interaction. You can build this client from source or you can integrate it into your firmware with the Yocto toolset.

When running on an embedded device, the Aktualizr client uses a minimal amount of memory and CPU and doesn’t need to remain resident in the memory at all. The client can any run on Linux-based operating system, or any operating system that includes the GNU C Library. However, if you run Aktualizr on a non-Linux system, you might have to customize it first. 

The client is responsible for the following tasks:

* Communicating with the HERE OTA Connect server
* Authenticating using locally available device and user credentials
* Reporting current software and hardware configuration to the server
* Checking for any available updates for the device
* Downloaded any available updates
* Installing the updates on the system, or notifying other services of the availability of the downloaded file
* Receiving or generating installation reports (success or failure) for attempts to install received software
* Submitting installation reports to the server

For all controllers that run aktualizr, you’ll need to implement some form of key provisioning. One option is to use a Hardware Security Module (HSM). The OTA Connect documentation explains in detail [how to set up an HSM](https://docs.atsgarage.com/prod/enable-implicit-provisioning.html) to hold potentially-sensitive provisioning key material.