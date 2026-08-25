#ifndef __VOICE_MODEL_CLIENT_HPP
#define __VOICE_MODEL_CLIENT_HPP

#include <stdint.h>

#define VOICE_MODEL_IP      "10.99.90.240"
#define VOICE_MODEL_PORT    8899
#define VOICE_MODEL_BUF_LEN 512

bool Voice_Model_Init(void);
bool Voice_Model_Send_Command(const char* text);
void Voice_Model_Poll(void);
bool Voice_Model_Handle_Local_Command(const char* action);

#endif
