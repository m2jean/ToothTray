# ToothTray

A tray icon in Windows task bar to quickly connect or disconnect bluetooth audio devices.

Right click the icon to see a list of paired bluetooth audio devices. Select a device to connect/disconnect them depending on their current connection state.

## Problem

Windows provides 2 ways to connect or disconnect bluetooth audio devices. One is using the "Connect" side pannel that can either be opened by pressing `Win + K` or from action center. Another requires opening the "Bluetooth and other devices" setting page. Both ways requires multiple clicks or key presses.

This project shows a way to connect and disconnect bluetooth audio devices programatically, which Windows doesn't provide any simple API for. With this developers can develop programs to connect bluetooth audio devices with keyboard shortcuts, or integrate the functionality into other programs, or even automatically connect when the bluetooth device is in range.

## Solution

After failing to find a solution by Googling, I found a way to control bluetooth audio device connection based on how Win 10's setting program does it. Related code are in `BluetoothAudioDevices`.

Instead of using Windows bluetooth API, [Windows Core Audio API](https://docs.microsoft.com/en-us/windows/win32/coreaudio/core-audio-apis-in-windows-vista) has to be used to connect bluetooth audio. Ultimately it is the driver that connect the bluetooth device to Windows' audio system, and we need to get an interface to the bluetooth audio driver.

After bluetooth audio devices are paired and the drivers are installed, they will appear in Windows as audio endpoint devices, which can be enumerated with [EnumAudioEndpoints](https://docs.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-enumaudioendpoints). Each endpoint has various properties. Their avalibility depends on the type of the endpoint. We can enumerate the properties programmatically, or view them in the details tab of a device from Device Manager.

One useful property is the `state` of an endpoint, which tells us whether a bluetooth audio device is connected. If it is connected, the state will be `DEVICE_STATE_ACTIVE`. Otherwise, it will be `DEVICE_STATE_UNPLUGGED`.

After getting the endpoints, we need to use the device topology API to get an interface to the driver. In Windows Driver Model (WDM), drivers expose the hardware and their functionality with a graph of kernel streaming (KS) components. By navgating through the topology, we can find the KS components that actually controls the connection of a bluetooth audio device.

And it turns out the the KS component we are looking for is always a KS filter directed connected to an audio endpoint (which might have been specified by related WDM audio driver development documents). We can get the filter by getting the connector of the filter from the connector of the endpoint. Then we can use the `IKsControl` interface of the filter to send `KSPROPSETID_BtAudio` property request to the driver, which is simlar to IO requests. By sending either `KSPROPERTY_ONESHOT_RECONNECT` or `KSPROPERTY_ONESHOT_DISCONNECT`, we ask the driver to connect or disconnect from the bluetooth audio device.

### Group and Identify Bluetooth Audio Endpoints

Sometimes a bluetooth audio device supports multiple bluetooth profiles, and it can appears in Windows as multiple different audio endpoints. When Windows connects to a bluetooth audio device, it connects all associated audio endpoints so that they can be switched on demand. And only when all associated audio endpoints are disconnected can a bluetooth audio be completed disconnected from Windows.

In Windows, devices and services are grouped by device containers that represent the actual physical devices. We can group the audio endpoints using their `PKEY_Device_ContainerId` property such that endpoints with the same container id are grouped together. I used WinRT `DeviceInformation::FindAllAsync` to enumerate all existing device containers and their properties. By joining them together, we get a unified representation of a connectable bluetooth audio device, with a name for the physical device (from the container) and multiple associated connection controls.

A final minor problem is to identify bluetooth audio devices, since both device containers and audio endpoints are not limited to bluetooth devices. I cheesed it by looking at the hardware id of the KS filters, but in production hardware ids are not reliable and are subject to change by Windows. A better way is to look at the properties of the containers or the endpoints for bluetooth exclusive properties. But the proper way is probably to enumerate bluetooth devices first and find their containers. Then use the containers to identify bluetooth audio endpoints.

## Unused Code and Discussions

A lot of code in this project are not used. They are from my attempts to solve the problem with Windows bluetooth API before looking at the audio API.

`BluetoothDeviceWatcher` is based Microsoft's [Device Enumeration and Pairing](https://github.com/microsoft/windows-universal-samples/tree/main/Samples/DeviceEnumerationAndPairing) sample. It uses WinRT API to monitor bluetooth devices. Originally I was hoping it can detect if a bluetooth device is in range, but the watcher only works when actively scanning for devices. This is actually a limit in the classic Bluetooth protocol, while Bluetooth LE provides a better way to monitor devices.

`BluetoothRadio` uses a method to connect and disconnect bluetooth devices described in both Stack Overflow and AutoHotKey community, by enabling and removing all associated services of a bluetooth devices. This method is slow for 2 reasons. Firstly, it has to initiate a bluetooth discovery which lasts for at leat 1.28 seconds. Secondly, modifying the service is almost like installing/uninstalling the driver and it takes some time. What's more, different bluetooth devices can have different services installed and we need a way to remember the services because once they are removed, there's no way to enumerate them anymore. Win32 also provides an API to monitor bluetooth device changes, but similar to the WinRT one, it requires actively scanning to work.

`BluetoothSocket` is a lower level examination of the bluetooth protocol, which Windows exposes via WinSock2. Using WinSock2, it does bluetooth devices and services discovery pretty much manually. The latter requires understading Bluetooth Service Discovery Protocol (SDP). It also has the ability to directly communicate to a bluetooth device, but this is probably what a bluetooth audio driver should be doing. Nevertheless, being able to initiate SDP with a device provides us a way to actively determine if a device is in range. By doing the discovery periodically it allows us to update the presence of a device. The only concern is, as an abuse to classic bluetooth protocol, how much extra energy it requries for both the local bluetooth ratio and the remote bluetooth device.
