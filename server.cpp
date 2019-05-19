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
#include <thread>
#include <chrono>
#include <assert.h>
#define MINIMP3_ONLY_MP3
/*#define MINIMP3_ONLY_SIMD*/
/*#define MINIMP3_NONSTANDARD_BUT_LOGICAL*/
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#include "minimp3_ex.h"
#include "serial.h"
using namespace std;
typedef struct {
	int16_t* data;
	int size;
} sound_t;
map<string, sound_t> sounds;

// #define WITH_SDL
#ifndef WITH_SDL
#define Uint8 void
#define SERIAL_PORT "/dev/ttyUSB0"
serial::Serial ser(SERIAL_PORT, 460800, serial::Timeout::simpleTimeout(1000));
#define INBUF_SIZE 128  // should be small
unsigned char inbuf[INBUF_SIZE];
uint16_t u16buf[INBUF_SIZE];
string indat;
#endif

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
	unsigned int ID;  // used to delete play object
	play_t(const sound_t& _sound, unsigned int _ID, float _scale=1): sound(_sound), ID(_ID), idx(0), scale(_scale) {}
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
	for (int i=0; i<len/2; ++i) data[i] &= ~0x000F;  // 16bit -> 12bit
	plays.erase(remove_if(plays.begin(), plays.end(), [](play_t& play) { return play.idx >= play.sound.size; }), plays.end());
	plays_mutex.unlock();

	// for (int i=0; i<len/2; ++i) {
	// 	data[i] = 32768 + sin(3.1415926535 * i / 8) * 30000;
	// }
}

void load_mp3_childdirs(const char* dirpath, const char* dirname);  // this will call load_mp3s

int main(int argc, char* argv[]) {
	srand((int)time(0));

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
	want.samples = 128;
	want.callback = MyAudioCallback;
	want.userdata = NULL;  // pass here for user-defined data
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);  // allow no change, SDL lib will automatically adapte the parameters
	if (dev <= 0) {
        printf("error: couldn't open audio: %s\n", SDL_GetError()); return 0;
    }
	SDL_PauseAudioDevice(dev, 0);  // play! this will call callback function
#else
	// initialize serial here, to call MyAudioCallback if data needed
	thread serth([&] {
		while (ser.available()) {
			int readlen = ser.available();
			if (readlen > INBUF_SIZE) readlen = INBUF_SIZE;
			ser.read(inbuf, readlen);
			printf("clear %d bytes from serial buffer\n", readlen);
		}
		printf("clear serial buffer done\n");
		while (1) {
			int len = ser.read(inbuf, INBUF_SIZE);
			if (len == 0) {
				printf("read timeout\n"); continue;
			}
			// printf("len = %d\n", len);
			int acquire_audio_len = 0;
			for (int i=0; i<len; ++i) {
				if (inbuf[i] == 0x8F) ++acquire_audio_len;
				else {
					// printf("inbuf[i] = %d\n", (int)inbuf[i]);
					assert(inbuf[i] >= 0 && inbuf[i] < 0x80 && "invalid special char");
					indat.append(1, inbuf[i]);
				}
			}
			for (int i=0; i<indat.length(); ++i) {  // handle one command
				if (indat[i] == '\n') {
					string cmd = indat.substr(0, i);
					printf("cmd: %s\n", cmd.c_str());
					indat = indat.substr(i+1);
					// explain the cmd
					if (cmd[0] == 'p' && cmd.length() == 2) {  // keyboard on C8051
						char c = cmd[1];
						assert(((c>='0'&&c<='9') || (c>='A'&&c<='F')) && "invalid key" );
						int keynum = c>='0'&&c<='9' ? c-'0' : c-'A'+10;
						// printf("keynum = %d\n", keynum);
						if (keynum >= 1 && keynum <= 7) {
							static const char* keys[] = {"piano/41!.mp3", "piano/42!.mp3", "piano/43!.mp3", "piano/44!.mp3",
								"piano/45!.mp3", "piano/46!.mp3", "piano/47!.mp3"};
							play_add(keys[keynum-1]);
						} else if (keynum == 0) {
							play_add("piano/52!.mp3");
						} else if (keynum == 9) {
							play_add("piano/51!.mp3");
						} else if (keynum > 9) {
							static const char* keys[] = {"piano/53!.mp3", "piano/54!.mp3", "piano/55!.mp3", "piano/56!.mp3",
								"piano/57!.mp3", "piano/61!.mp3"};
							play_add(keys[keynum-10]);
						}
					}
				}
			}
			// printf("acquire_audio_len = %d\n", acquire_audio_len);
			MyAudioCallback(NULL, (void*)u16buf, acquire_audio_len*2);
			for (int i=0; i<acquire_audio_len; ++i) {  // re-format data to send
				uint16_t dat = u16buf[i] >> 4;
				unsigned char* ptr = (unsigned char*)&u16buf[i];
				ptr[0] = 0x80 | (dat >> 6);
				ptr[1] = 0xC0 | (dat & 0x3F);
			}
			ser.write((uint8_t*)u16buf, acquire_audio_len*2);
		}
	});
#endif

	// play_add("piano/41!.mp3", 1);
	// sleep(1);
	// play_add("piano/51!.mp3", 1);
	// sleep(1);
	// play_add("piano/61!.mp3", 1);  // test OK !!!

	mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

void log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
    printf("%s\n", str);  // this is too much!!!
}

void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	static char topicbuf[32];
    static char msgbuf[256];
	if (strncmp(message->topic, "band/", 5) == 0) {
		char* subtopic = message->topic + 5;
		if (strcmp(subtopic, "query") == 0) {  // reply to query
			mosquitto_publish(mosq, NULL, "band/info", strlen(VERSION_STR), VERSION_STR, USEQOS, 0);
		} else if (strlen(subtopic) >= 7 && strlen(subtopic) < sizeof(topicbuf) && message->payloadlen < sizeof(msgbuf) && subtopic[6] == '/') {
			char* clientid = subtopic;
			clientid[6] = '\0';  // this will modify message->topic !!!!
			char* subsubtopic = subtopic + 7;
			memcpy(msgbuf, message->payload, message->payloadlen);  // copy here
			msgbuf[message->payloadlen] = '\0';
			printf("clientid %s[%s]: %s\n", clientid, subsubtopic, msgbuf);
			sprintf(topicbuf, "band/%s/", clientid);
			if (strcmp(subsubtopic, "play") == 0) {
				int ID = play_add(msgbuf, 1);
				strcat(topicbuf, "start");
				sprintf(msgbuf, "%d", ID);
				mosquitto_publish(mosq, NULL, topicbuf, strlen(msgbuf), msgbuf, USEQOS, 0);
			} else if (strcmp(subsubtopic, "stop") == 0) {
				strcat(topicbuf, "end");
				sprintf(msgbuf, "%d", play_remove(atoi(msgbuf)));
				mosquitto_publish(mosq, NULL, topicbuf, strlen(msgbuf), msgbuf, USEQOS, 0);
			}
			clientid[6] = '/';  // return
		}
	}
}

void connect_callback(struct mosquitto *mosq, void *userdata, int result) {
    if (!result) {
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, "band/query", USEQOS);
		// the following is related to clientid, which is exactly 6 byte name
		mosquitto_subscribe(mosq, NULL, "band/+/register", USEQOS);  // register with prefered scale, return band/+/scale: scale now
		mosquitto_subscribe(mosq, NULL, "band/+/play", USEQOS);  // play sound, return band/+/start: id
		mosquitto_subscribe(mosq, NULL, "band/+/stop", USEQOS);  // stop sound, return band/+/end: id
    } else {
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
		if (ptr->d_type != DT_DIR && ptr->d_name[0] != '.' && ptr->d_name[strlen(ptr->d_name)-1] != 'l') {  // filter out html
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
	// print memory usage
	int sum = 0;
	for (auto i=sounds.begin(); i!=sounds.end(); ++i) sum += i->second.size;
	printf("summary: %d samples in total, that is %fMB\n", sum, sum*2/1e6);
}

int find_idx_by_id(unsigned int ID) {
	for (int i=0; i<plays.size(); ++i) {
		if (plays[i].ID == ID) return i;
	}
	return -1;
}

int play_add(const sound_t& sound, float scale) {
	unsigned int ID;
	int i;
	plays_mutex.lock();
	for (i=0; i<5; ++i) {
		ID = rand();
		printf("try ID = %u\n", ID);
		if (find_idx_by_id(ID) != -1) continue;
		plays.push_back(play_t(sound, ID, scale));
		break;
	}
	plays_mutex.unlock();
	if (i < 5) return ID;
	return 0;
}

int play_add(const char* name, float scale) {
	if (sounds.find(name) != sounds.end()) {
		return play_add(sounds[name], scale);
	} return 0;
}

int play_remove(int ID) {
	int ret = 0;
	plays_mutex.lock();
	int idx = find_idx_by_id(ID);
	if (idx != -1) {
		plays.erase(plays.begin() + idx);
		ret = ID;
	}
	plays_mutex.unlock();
	return ret;
}
