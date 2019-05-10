# C8051band
multi-user music player on C8051F020

# installation

## deployment

you need to configure mqtt (with websocket) and nginx properly:

for `/etc/mosquitto/mosquitto.conf`:

```init
port 1883
protocol mqtt
listener 9000
protocol websockets
```

and for `/etc/nginx/sites-enabled/default` (the default http server):

```init
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

then visiting [http://<your ip>/band/]() you will see mqtt connected, but still searching for band server.

## development

For development, a testbed without MCU is high efficient, using libsdl to play the music and debug the system.

clone [minimp3 project](https://github.com/lieff/minimp3) to anywhere, and run `player/build.sh`, the script will download libsdl automatically and install for you.
