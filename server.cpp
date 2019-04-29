#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mosquitto.h"
#include <pthread.h>
#include <dirent.h>  // only linux
#include <math.h>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#define MINIMP3_ONLY_MP3
/*#define MINIMP3_ONLY_SIMD*/
/*#define MINIMP3_NONSTANDARD_BUT_LOGICAL*/
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#include "minimp3_ex.h"
using std::vector;
using std::map;
using std::make_pair;
using std::string;
using std::mutex;
using std::remove_if;
typedef struct {
	int16_t* data;
	int size;
} sound_t;
map<string, sound_t> sounds;

#define WITH_SDL

#define VERSION_STR "C8051band v0.0.1, compiled at " __TIME__ ", " __DATE__ 
#define USEQOS 0
const char* htmldir = "./html/";

struct mosquitto* mosq = NULL;
const char* HOST = "localhost";
int PORT = 1883;
void log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
void connect_callback(struct mosquitto *mosq, void *userdata, int result);
void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);

struct play_t {
	sound_t sound;
	int idx;
	float scale;
	int ID;  // used to delete play object
	play_t(const sound_t& _sound, float _scale=1): sound(_sound), idx(0), scale(_scale) {}
};
mutex plays_mutex;
vector<play_t> plays;
int play_add(const sound_t& sound, float scale=1);  // return the ID
int play_add(const char* name, float scale=1);  // return the ID
int play_remove(int ID);

#ifdef WITH_SDL
#include <SDL2/SDL.h>
#endif
void MyAudioCallback(void* userdata, Uint8* stream, int len) {
	// printf("stream %p, len is %d\n", stream, len);
	uint16_t* data = (uint16_t*)stream;
	for (int i=0; i<len/2; ++i) data[i] = 32768;
	plays_mutex.lock();
	for (int i=0; i<plays.size(); ++i) {
		play_t& play = plays[i];
		for (int j=0; j<len/2 && play.idx < play.sound.size; ++j, ++play.idx) {
			int sample = data[j] + play.sound.data[play.idx] * play.scale;
			if (sample > 65535) data[j] = 65535;
			else if (sample < 0) data[j] = 0;
			else data[j] = sample;  // avoid overflow noise
		}
	}
	plays.erase(remove_if(plays.begin(), plays.end(), [](play_t& play) { return play.idx >= play.sound.size; }), plays.end());
	plays_mutex.unlock();

	// for (int i=0; i<len/2; ++i) {
	// 	data[i] = 32768 + sin(3.1415926535 * i / 8) * 30000;
	// }
}

void load_mp3_childdirs(const char* dirpath, const char* dirname);  // this will call load_mp3s

int main(int argc, char* argv[]) {

	// init mqtt server
	mosquitto_lib_init();
	bool session = true;  // TODO what's this
	mosq = mosquitto_new(NULL, session, NULL);
	if(!mosq){
        mosquitto_lib_cleanup();
        perror("create mqtt client failed..\n");
    }
    printf("mqtt client connecting to %s:%d\n", HOST, PORT);
    if(mosquitto_connect(mosq, HOST, PORT, 60)){
        perror("Unable to connect.\n");
    }
    mosquitto_log_callback_set(mosq, log_callback);
    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_message_callback_set(mosq, message_callback);
	mosquitto_subscribe_callback_set(mosq, subscribe_callback);
    printf("mqtt client connect success\n");

	load_mp3_childdirs(htmldir, "");

#ifdef WITH_SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("error: sdl init failed: %s\n", SDL_GetError()); return 0;
    }
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID dev;
	want.freq = 8000;
	want.format = AUDIO_U16;  // uint16_t, MCU will use 12bit unsigned value
	want.channels = 1;
	want.samples = 512;
	want.callback = MyAudioCallback;
	want.userdata = NULL;  // pass here for user-defined data
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);  // allow no change, SDL lib will automatically adapte the parameters
	if (dev <= 0) {
        printf("error: couldn't open audio: %s\n", SDL_GetError()); return 0;
    }
	SDL_PauseAudioDevice(dev, 0);  // play! this will call callback function
#else
	// initialize serial here
#endif

	play_add("piano/41!.mp3", 1);
	sleep(1);
	play_add("piano/51!.mp3", 1);
	sleep(1);
	play_add("piano/61!.mp3", 1);

	mosquitto_loop_forever(mosq, -1, 1);
	
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

void load_mp3s(const char* dirpath, const char* dirname) {
	printf("loading directory %s as %s\n", dirpath, dirname);
	struct dirent * ptr;
	DIR* dir = opendir(dirpath);
	char path[NAME_MAX + 1];
	char name[NAME_MAX + 1];
	while ((ptr = readdir(dir)) != NULL) {
		if (ptr->d_type != DT_DIR && ptr->d_name[0] != '.') {
			strcpy(path, dirpath);
			strcat(path, ptr->d_name);
			strcpy(name, dirname);
			strcat(name, ptr->d_name);
			printf("  loading file %s as %s, ", path, name);
			mp3dec_t mp3d;
			mp3dec_file_info_t info;
			sounds.insert(make_pair(name, sound_t()));
			if (mp3dec_load(&mp3d, path, &info, NULL, NULL)) {
				perror("mp3dec_load error");
			}
			printf("size: %d\n", (int)info.samples);
			sound_t& sound = sounds[name];
			sound.data = info.buffer;
			sound.size = info.samples;
			// plays.push_back(play_t(sound));
		}
	}
}

void load_mp3_childdirs(const char* dirpath, const char* dirname) {  // name is used with mqtt, not related to real path
	struct dirent * ptr;
	DIR* dir = opendir(dirpath);
	char subdirpath[NAME_MAX + 1];
	char subdirname[NAME_MAX + 1];
	while ((ptr = readdir(dir)) != NULL) {
		if (ptr->d_type == DT_DIR && ptr->d_name[0] != '.') {
			strcpy(subdirpath, dirpath);
			strcat(subdirpath, ptr->d_name);
			strcat(subdirpath, "/");
			strcpy(subdirname, dirname);
			strcat(subdirname, ptr->d_name);
			strcat(subdirname, "/");
			load_mp3s(subdirpath, subdirname);
		}
		// printf("d_name : %s\n", ptr->d_name);
	}
}

int play_add(const sound_t& sound, float scale) {
	plays_mutex.lock();
	plays.push_back(play_t(sound, scale));
	plays_mutex.unlock();
	return 0;  // TODO: add ID
}

int play_add(const char* name, float scale) {
	if (sounds.find(name) != sounds.end()) {
		return play_add(sounds[name], scale);
	} return 0;
}

int play_remove(int ID) {

}
