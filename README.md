# NFC Spotify Player
An NFC based Spotify player that allows you to play music on your favorite Smart Devices. It uses the Official Spotify API for playing music on the connected devices. On the inside, it uses the **ESP8266 NodeMCU**, **MFRC522 RFID Reader**, and **KY-016 LED module** as an indicator. It has a built in web interface for WiFi Management and Spotify Authentication and Device Selection so that it could work without any additional dependencies. For the "CDs", it uses a keychain with a built in NFC tag that has the Spotify album/playlist URL.

## Images and Screenshots

### Spotify Box w/ Album Rack
<img alt="Spotify Box with Album Rack" src="https://raw.githubusercontent.com/janjanmedinaaa/nfc-spotify-player/master/images/box_with_rack.jpg" width="40%">

### Spotify Authentication and Device Selection
<img alt="Spotify Authentication" src="https://raw.githubusercontent.com/janjanmedinaaa/nfc-spotify-player/master/images/home.jpg" width="40%">
&nbsp; &nbsp; &nbsp; &nbsp;
<img alt="Spotify Device Selection" src="https://raw.githubusercontent.com/janjanmedinaaa/nfc-spotify-player/master/images/device_selection.jpg" width="40%">

## Libraries and References
- [ESP32 Spotify Echo Dot](https://github.com/makeratplay/esp32SpotifyEchoDot)
- [Spotify API Library](https://github.com/witnessmenow/spotify-api-arduino)
- [RFID Library](https://github.com/miguelbalboa/rfid)