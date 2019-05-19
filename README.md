# C8051band
multi-user music player on C8051F020

# installation

## deployment

you need to configure mqtt (with websocket) and nginx properly:

`sudo apt install nginx mosquitto-dev mosquitto-clients`

for `/etc/mosquitto/mosquitto.conf`:

```ini
port 1883
protocol mqtt
listener 9000
protocol websockets
```

and for `/etc/nginx/sites-enabled/default` (the default http server):

```ini
# this is to server your file
location ~ ^/band(.*)$ {
	root /home/wuyue/Documents/C8051band/html;  # change this to your location
	try_files $1 $1index.html = 404;
}

# this is to proxy your mqtt websocket connection
location ~ ^\/mqtt$ {
	proxy_pass http://127.0.0.1:9000;
	proxy_http_version 1.1;
	proxy_set_header Upgrade $http_upgrade;
	proxy_set_header Connection "upgrade";
}

```

then visiting [http://<your ip>/band/]() you will see MQTT connected, but still searching for band server.

next, to optimize system performance (especially latency), set the process to a real-time one.

run the program with root permission `sudo ./server` then use `top` you will get this

```init
 7888 root      rt   0  153516  24544   9392 S   3.7  0.3   0:01.18 server
```

the `rt` means this is a real-time process now.

(you may see `Home directory not accessible: Permission denied` but it doesn't matter)

## development

For development, a testbed without MCU is high efficient, using `libsdl` to play the music and debug the system.

clone [minimp3 project](https://github.com/lieff/minimp3) to anywhere, and run `player/build.sh`, the script will download libsdl automatically and install for you.

`sudo apt install libsdl2-2.0-0 libsdl2-dev`

