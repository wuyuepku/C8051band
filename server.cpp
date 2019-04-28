#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mosquitto.h"
#include <pthread.h>
#define MINIMP3_ONLY_MP3
/*#define MINIMP3_ONLY_SIMD*/
/*#define MINIMP3_NONSTANDARD_BUT_LOGICAL*/
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#include "minimp3_ex.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define VERSION_STR "C8051band v0.0.1, compiled at " __TIME__ ", " __DATE__ 
#define USEQOS 0

struct mosquitto* mosq = NULL;
const char* HOST = "localhost";
int PORT = 1883;
void log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
void connect_callback(struct mosquitto *mosq, void *userdata, int result);
void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);

int main(int argc, char* argv[]) {
	// init mqtt server
	mosquitto_lib_init();
	bool session = true;  // TODO what's this
	mosq = mosquitto_new(NULL, session, NULL);
	if(!mosq){
        fprintf(stderr, "create mqtt client failed..\n");
        mosquitto_lib_cleanup();
        exit(1);
    }
    printf("mqtt client connecting to %s:%d\n", HOST, PORT);
    if(mosquitto_connect(mosq, HOST, PORT, 60)){
        fprintf(stderr, "Unable to connect.\n");
        exit(2);
    }
    mosquitto_log_callback_set(mosq, log_callback);
    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_message_callback_set(mosq, message_callback);
	mosquitto_subscribe_callback_set(mosq, subscribe_callback);
    printf("mqtt client connect success\n");

	mp3dec_t mp3d;
	mp3dec_init(&mp3d);
	mp3dec_frame_info_t info;
	short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	samples = mp3dec_decode_frame(&mp3d, input_buf, buf_size, pcm, &info);

	// mosquitto_loop_forever(mosq, -1, 1);
	
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

void log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
    printf("%s\n", str);  // this is too much!!!
}

void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    static char msgbuf[256];
    if (strcmp(message->topic, "band/query") == 0) {  // reply to query
        mosquitto_publish(mosq, NULL, "band/info", strlen(VERSION_STR), VERSION_STR, USEQOS, 0);
    }
}

void connect_callback(struct mosquitto *mosq, void *userdata, int result) {
    int i;
    if(!result){
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, "band/query", USEQOS);
    }else{
        fprintf(stderr, "Connect failed\n");
    }
}
 
void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
    int i;
    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for(i=1; i<qos_count; i++){
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}
