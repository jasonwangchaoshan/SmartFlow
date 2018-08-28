# SmartFlow
a device for measuring gas flow


**Note**

1.This example uses an 8Mbyte flash chip.
we use more than 1.4Mbyte of flash memory.

**Here is  the “Factory app, two OTA definitions” configuration:**

![ESP32 Partition Table](https://github.com/DanielXie00/SmartFlow/blob/master/others/flash.png)

2.
When flashing, use the `erase_flash` target first to erase the entire flash (this deletes any leftover data in the ota_data partition). Then flash the factory image over serial:

```
make erase_flash flash
```

(The `make erase_flash flash` means "erase everything, then flash". `make flash` only erases the parts of flash which are being rewritten.)


# Here are some of the tools that might be used


*Android version: https://github.com/DanielXie00/EsptouchForAndroid/tree/master/releases/apk

*IOS version:     https://github.com/DanielXie00/EsptouchForIOS

And **installed** on the phone, its role is to send wifi and password to the device

Open the EspTouch software, fill in the wifi password, then click "CONFIRM"
![EspTouch](/others/EspTouch.png)


Any suggestions please send to **xieshupei2010@gmail.com**,thank you.

























