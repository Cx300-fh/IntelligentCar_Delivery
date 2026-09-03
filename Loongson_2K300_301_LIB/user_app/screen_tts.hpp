/**
 * @file screen_tts.hpp
 * @brief 在线TTS到陶晶驰屏幕喇叭；与voice.cpp车载喇叭完全独立。
 */
#ifndef __SCREEN_TTS_HPP
#define __SCREEN_TTS_HPP

#include <string>

// 启动/停止后台工作线程。初始化不会自动播放任何内容。
bool ScreenTTS_Init();
void ScreenTTS_Shutdown();

// 非阻塞入队。文本由TTSMaker合成后上传到ram/tts.wav并在屏幕端播放。
bool ScreenTTS_Speak(const std::string& utf8_text);
bool ScreenTTS_Busy();

#endif
