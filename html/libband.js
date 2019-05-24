function randomID() {
	let length = 6;
	let chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
	var result = '';
	for (var i = length; i > 0; --i) result += chars[Math.floor(Math.random() * chars.length)];
	return result;
}
var mqtt;
var host = document.domain;  // 192.168.1.1
var port = 80;
var useSSL = false;
var topic = 'band/#';
var clientid = randomID();
function set_status(color, html) {
	$("#status").css("color", color);
	$("#status").html(html);
}
function mqtt_connect() {
	set_status("green", "connected");
	mqtt.subscribe(topic);
	let message = new Paho.MQTT.Message("who?");
	message.destinationName = "band/query";
	mqtt.send(message);
}
var cmdcnt = 0;
function mqtt_send(topic, payload) {
	let message = new Paho.MQTT.Message(payload);
	message.destinationName = topic;
	mqtt.send(message);
}
function mqtt_connection_lost() {
	set_status("red", "connection broken");
}
function mqtt_failure() {
	set_status("red", "connection failed");
}
function band_init() {
	set_status("red", "initializing...");
	if (getpara("host") != null) host = getpara("host");
	if (getpara("port") != null) port = parseInt(getpara("port"));
	if (getpara("useSSL") != null) useSSL = getpara("useSSL");
	if (getpara("topic") != null) topic = getpara("topic");
	if (getpara("clientid") != null) clientid = getpara("clientid");
	$("#clientid").html(clientid);
	mqtt = new Paho.MQTT.Client(host, port, clientid);  // the last parameter is clientid, using topic instead
	mqtt.onConnectionLost = mqtt_connection_lost;
	mqtt.onMessageArrived  = mqtt_message_arrive;
	mqtt.connect({
		useSSL, mqttVersion: 4, timeout: 3, keepAliveInterval: 10,
		onSuccess: mqtt_connect,
		onFailure: mqtt_failure,
	});
}
function getpara(name) {
	var reg = new RegExp("(^|&)"+ name +"=([^&]*)(&|$)");
	var r = window.location.search.substr(1).match(reg);
	if(r!=null) return unescape(r[2]); return null;
}

