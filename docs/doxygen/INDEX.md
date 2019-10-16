Aktualizr is the HERE OTA Connect client. This client runs on any embedded device and can check for updates periodically or triggered by another system interaction. You can build this client from source or you can integrate it into your firmware with the Yocto toolset.

When running on an embedded device, the aktualizr client uses a minimal amount of memory and CPU and doesn’t need to remain resident in the memory at all. The client can run on any Linux-based operating system, or any operating system that includes the GNU C Library. However, if you run aktualizr on a non-Linux system, you might have to customize it first. 

The client is responsible for the following tasks:

* Communicating with the HERE OTA Connect server
* Authenticating using locally available device and user credentials
* Reporting current software and hardware configuration to the server
* Checking for any available updates for the device
* Downloaded any available updates
* Installing the updates on the system, or notifying other services of the availability of the downloaded file
* Receiving or generating installation reports (success or failure) for attempts to install received software
* Submitting installation reports to the server

The aktualizr client application is a thin wrapper around the client library "libaktualizr". You could regard this library as a kind of toolbox. You can use the parts in this library to build a software update solution that conforms to the Uptane standard. 

For all controllers that run aktualizr or include libaktualizr, you’ll need to implement some form of key provisioning. The OTA Connect documentation explains in detail how to [select a provisioning method](https://docs.ota.here.com/ota-client/latest/client-provisioning-methods.html) that suits your use case. For more information on how you can use this library, also see the [reference docs](https://github.com/advancedtelematic/aktualizr/tree/master/docs).
