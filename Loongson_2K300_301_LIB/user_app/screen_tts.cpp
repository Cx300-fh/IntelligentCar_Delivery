/**
 * @file screen_tts.cpp
 * @brief TTSMaker在线合成 -> PCM WAV 22050Hz -> 陶晶驰RAM文件系统。
 * @note  本模块不包含、不调用voice.cpp，屏幕和车载喇叭互不影响。
 */
#include "screen_tts.hpp"
#include "screen.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

const char* const kCreateOrderUrl = "https://api.ttsmaker.cn/v1/create-tts-order";
const char* const kCurlPath = "/root/screen_tts_runtime/curl";
const char* const kCaBundlePath = "/root/screen_tts_runtime/cacert.pem";
const char* const kDefaultToken = "ttsmaker_demo_token";
const char* const kDefaultVoice = "1504";       // 普通话女声“潇潇”
const uint32_t kScreenSampleRate = 22050;
const size_t kDefaultMaxWavBytes = 180 * 1024;  // 配合HMI的192KB RAM文件区
const size_t kQueueLimit = 8;

std::atomic<bool> g_running(false);
std::atomic<bool> g_busy(false);
std::thread g_worker;
std::mutex g_queue_mutex;
std::condition_variable g_queue_cv;
std::deque<std::string> g_queue;

uint16_t read_le16(const std::vector<uint8_t>& b, size_t p)
{
    return (uint16_t)(b[p] | ((uint16_t)b[p + 1] << 8));
}

uint32_t read_le32(const std::vector<uint8_t>& b, size_t p)
{
    return (uint32_t)b[p] | ((uint32_t)b[p + 1] << 8) |
           ((uint32_t)b[p + 2] << 16) | ((uint32_t)b[p + 3] << 24);
}

void append_le16(std::vector<uint8_t>* b, uint16_t value)
{
    b->push_back((uint8_t)(value & 0xFF));
    b->push_back((uint8_t)(value >> 8));
}

void append_le32(std::vector<uint8_t>* b, uint32_t value)
{
    b->push_back((uint8_t)(value & 0xFF));
    b->push_back((uint8_t)((value >> 8) & 0xFF));
    b->push_back((uint8_t)((value >> 16) & 0xFF));
    b->push_back((uint8_t)(value >> 24));
}

bool make_temp_file(std::string* path, int* fd)
{
    char name[] = "/tmp/screen_tts_XXXXXX";
    int local_fd = mkstemp(name);
    if (local_fd < 0) return false;
    fchmod(local_fd, 0600);
    *path = name;
    *fd = local_fd;
    return true;
}

bool write_all(int fd, const std::string& text)
{
    size_t offset = 0;
    while (offset < text.size()) {
        ssize_t n = write(fd, text.data() + offset, text.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        offset += (size_t)n;
    }
    return true;
}

bool run_program(const std::vector<std::string>& args)
{
    if (args.empty()) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        std::vector<char*> argv;
        for (size_t i = 0; i < args.size(); ++i)
            argv.push_back(const_cast<char*>(args[i].c_str()));
        argv.push_back(NULL);
        execv(argv[0], &argv[0]);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool read_file(const std::string& path, std::vector<uint8_t>* data)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size <= 0 || size > 16 * 1024 * 1024) return false;
    in.seekg(0, std::ios::beg);
    data->resize((size_t)size);
    return (bool)in.read((char*)&(*data)[0], size);
}

bool read_text_file(const std::string& path, std::string* text)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    text->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return !text->empty() && text->size() < 1024 * 1024;
}

bool curl_post_json(const std::string& payload_path, const std::string& output_path)
{
    std::vector<std::string> args;
    args.push_back(kCurlPath);
    args.push_back("--silent");
    args.push_back("--show-error");
    args.push_back("--fail-with-body");
    args.push_back("--connect-timeout");
    args.push_back("10");
    args.push_back("--max-time");
    args.push_back("30");
    args.push_back("--retry");
    args.push_back("1");
    args.push_back("--cacert");
    args.push_back(kCaBundlePath);
    args.push_back("--header");
    args.push_back("Content-Type: application/json; charset=utf-8");
    args.push_back("--data-binary");
    args.push_back(std::string("@") + payload_path);
    args.push_back("--output");
    args.push_back(output_path);
    args.push_back(kCreateOrderUrl);
    return run_program(args);
}

bool curl_download(const std::string& url, const std::string& output_path)
{
    if (url.compare(0, 8, "https://") != 0) return false;
    std::vector<std::string> args;
    args.push_back(kCurlPath);
    args.push_back("--silent");
    args.push_back("--show-error");
    args.push_back("--fail");
    args.push_back("--location");
    args.push_back("--connect-timeout");
    args.push_back("10");
    args.push_back("--max-time");
    args.push_back("30");
    args.push_back("--retry");
    args.push_back("1");
    args.push_back("--cacert");
    args.push_back(kCaBundlePath);
    args.push_back("--output");
    args.push_back(output_path);
    args.push_back(url);
    return run_program(args);
}

bool synthesize_wav(const std::string& text, std::vector<uint8_t>* wav)
{
    // 仅供串口/RAM/音频控件联调。正常启动不设置该变量，仍使用在线TTS。
    const char* local_test_wav = getenv("SCREEN_TTS_TEST_WAV");
    if (local_test_wav && *local_test_wav) {
        if (read_file(local_test_wav, wav)) {
            printf("[ScreenTTS] 诊断模式：读取本地WAV %s（不消耗Token）\n",
                   local_test_wav);
            return true;
        }
        printf("[ScreenTTS] 诊断模式：无法读取本地WAV %s\n", local_test_wav);
        return false;
    }
    const char* token_env = getenv("TTSMAKER_TOKEN");
    const char* voice_env = getenv("TTSMAKER_VOICE_ID");
    const std::string token = (token_env && *token_env) ? token_env : kDefaultToken;
    const std::string voice = (voice_env && *voice_env) ? voice_env : kDefaultVoice;

    nlohmann::json request;
    request["token"] = token;
    request["text"] = text;
    request["voice_id"] = voice;
    request["audio_format"] = "wav";
    request["audio_speed"] = 1.0;
    request["audio_volume"] = 0;
    request["text_paragraph_pause_time"] = 0;

    std::string request_path, response_path, wav_path;
    int request_fd = -1, response_fd = -1, wav_fd = -1;
    if (!make_temp_file(&request_path, &request_fd) ||
        !make_temp_file(&response_path, &response_fd) ||
        !make_temp_file(&wav_path, &wav_fd)) {
        if (request_fd >= 0) close(request_fd);
        if (response_fd >= 0) close(response_fd);
        if (wav_fd >= 0) close(wav_fd);
        if (!request_path.empty()) unlink(request_path.c_str());
        if (!response_path.empty()) unlink(response_path.c_str());
        if (!wav_path.empty()) unlink(wav_path.c_str());
        return false;
    }
    close(response_fd);
    close(wav_fd);
    const std::string request_text = request.dump();
    bool ok = write_all(request_fd, request_text);
    close(request_fd);

    std::string response_text;
    std::string audio_url;
    try {
        ok = ok && curl_post_json(request_path, response_path) &&
             read_text_file(response_path, &response_text);
        if (ok) {
            nlohmann::json response = nlohmann::json::parse(response_text);
            if (response.contains("audio_file_url"))
                audio_url = response["audio_file_url"].get<std::string>();
            else if (response.contains("data") && response["data"].contains("audio_file_url"))
                audio_url = response["data"]["audio_file_url"].get<std::string>();
            if (audio_url.empty()) {
                std::string message = response.value("error_details",
                    response.value("msg", std::string("无下载地址")));
                printf("[ScreenTTS] TTSMaker拒绝请求：%s\n", message.c_str());
                ok = false;
            }
        }
    } catch (const std::exception& e) {
        printf("[ScreenTTS] TTSMaker响应解析失败：%s\n", e.what());
        ok = false;
    }
    ok = ok && curl_download(audio_url, wav_path) && read_file(wav_path, wav);

    unlink(request_path.c_str());
    unlink(response_path.c_str());
    unlink(wav_path.c_str());
    return ok;
}

bool convert_pcm_wav_22050(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>* output, double* duration_seconds)
{
    if (input.size() < 44 || memcmp(&input[0], "RIFF", 4) != 0 ||
        memcmp(&input[8], "WAVE", 4) != 0) return false;

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0;
    size_t data_pos = 0, data_size = 0;
    for (size_t p = 12; p + 8 <= input.size();) {
        uint32_t chunk_size = read_le32(input, p + 4);
        size_t next = p + 8 + chunk_size + (chunk_size & 1U);
        if (next > input.size()) return false;
        if (memcmp(&input[p], "fmt ", 4) == 0 && chunk_size >= 16) {
            format = read_le16(input, p + 8);
            channels = read_le16(input, p + 10);
            sample_rate = read_le32(input, p + 12);
            bits = read_le16(input, p + 22);
        } else if (memcmp(&input[p], "data", 4) == 0) {
            data_pos = p + 8;
            data_size = chunk_size;
        }
        p = next;
    }
    if (format != 1 || channels != 1 || bits != 16 || sample_rate == 0 ||
        data_pos == 0 || data_size < 2 || data_pos + data_size > input.size()) return false;

    const size_t in_count = data_size / 2;
    std::vector<int16_t> samples(in_count);
    for (size_t i = 0; i < in_count; ++i)
        samples[i] = (int16_t)read_le16(input, data_pos + i * 2);

    size_t out_count = (size_t)llround((double)in_count * kScreenSampleRate / sample_rate);
    if (out_count == 0) return false;
    std::vector<int16_t> resampled(out_count);
    if (sample_rate == kScreenSampleRate) {
        resampled = samples;
    } else {
        const double ratio = (double)sample_rate / kScreenSampleRate;
        for (size_t i = 0; i < out_count; ++i) {
            double source = i * ratio;
            size_t left = (size_t)source;
            if (left >= in_count - 1) {
                resampled[i] = samples[in_count - 1];
            } else {
                double fraction = source - left;
                double value = samples[left] * (1.0 - fraction) + samples[left + 1] * fraction;
                value = std::max(-32768.0, std::min(32767.0, value));
                resampled[i] = (int16_t)lrint(value);
            }
        }
    }

    output->clear();
    output->reserve(44 + resampled.size() * 2);
    output->insert(output->end(), {'R','I','F','F'});
    append_le32(output, (uint32_t)(36 + resampled.size() * 2));
    output->insert(output->end(), {'W','A','V','E','f','m','t',' '});
    append_le32(output, 16);
    append_le16(output, 1);
    append_le16(output, 1);
    append_le32(output, kScreenSampleRate);
    append_le32(output, kScreenSampleRate * 2);
    append_le16(output, 2);
    append_le16(output, 16);
    output->insert(output->end(), {'d','a','t','a'});
    append_le32(output, (uint32_t)(resampled.size() * 2));
    for (size_t i = 0; i < resampled.size(); ++i)
        append_le16(output, (uint16_t)resampled[i]);
    *duration_seconds = (double)resampled.size() / kScreenSampleRate;
    return true;
}

size_t max_wav_bytes()
{
    const char* value = getenv("SCREEN_TTS_MAX_BYTES");
    if (!value || !*value) return kDefaultMaxWavBytes;
    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (!end || *end != '\0' || parsed < 64 * 1024 || parsed > 512 * 1024)
        return kDefaultMaxWavBytes;
    return (size_t)parsed;
}

bool split_utf8(const std::string& text, std::string* left, std::string* right)
{
    std::vector<size_t> boundaries;
    for (size_t p = 0; p < text.size();) {
        boundaries.push_back(p);
        unsigned char c = (unsigned char)text[p];
        size_t step = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
        p = std::min(text.size(), p + step);
    }
    if (boundaries.size() < 2) return false;
    size_t split = boundaries[boundaries.size() / 2];
    // 优先在中点附近的标点后切分，语音更自然。
    const char* punct[] = {"。", "！", "？", "，", ";", ",", ".", "!", "?"};
    size_t best = std::string::npos;
    size_t best_distance = text.size();
    for (size_t i = 0; i < sizeof(punct) / sizeof(punct[0]); ++i) {
        size_t pos = text.find(punct[i]);
        while (pos != std::string::npos) {
            size_t after = pos + strlen(punct[i]);
            if (after > 0 && after < text.size()) {
                size_t distance = (after > split) ? after - split : split - after;
                if (distance < best_distance) { best = after; best_distance = distance; }
            }
            pos = text.find(punct[i], pos + 1);
        }
    }
    if (best != std::string::npos && best_distance < text.size() / 3) split = best;
    *left = text.substr(0, split);
    *right = text.substr(split);
    return !left->empty() && !right->empty();
}

bool wait_playback(double seconds)
{
    std::unique_lock<std::mutex> lock(g_queue_mutex);
    return g_queue_cv.wait_for(lock, std::chrono::milliseconds((int)(seconds * 1000 + 300)),
                               [](){ return !g_running.load(); });
}

bool process_text(const std::string& text, int depth)
{
    std::vector<uint8_t> source_wav, screen_wav;
    double duration = 0.0;
    printf("[ScreenTTS] 正在在线合成（%zu字节文本）\n", text.size());
    if (!synthesize_wav(text, &source_wav)) {
        printf("[ScreenTTS] 在线合成或下载失败，请检查网络/Token\n");
        return false;
    }
    if (!convert_pcm_wav_22050(source_wav, &screen_wav, &duration)) {
        printf("[ScreenTTS] 返回音频不是PCM16单声道WAV\n");
        return false;
    }
    if (!g_running.load()) return false;
    if (screen_wav.size() > max_wav_bytes()) {
        std::string left, right;
        if (depth >= 5 || !split_utf8(text, &left, &right)) {
            printf("[ScreenTTS] 音频%zu字节超过RAM文件区限制%zu字节\n",
                   screen_wav.size(), max_wav_bytes());
            return false;
        }
        printf("[ScreenTTS] 音频较长，拆分为两段播放\n");
        return process_text(left, depth + 1) && g_running.load() &&
               process_text(right, depth + 1);
    }
    if (!Screen_Upload_Wav_And_Play(&screen_wav[0], screen_wav.size())) {
        printf("[ScreenTTS] 上传/播放失败，请检查HMI的wav_tts和RAM文件区\n");
        return false;
    }
    printf("[ScreenTTS] 屏幕开始播放 %.2f 秒（%zu字节）\n", duration, screen_wav.size());
    wait_playback(duration);
    return true;
}

void worker_main()
{
    while (g_running.load()) {
        std::string text;
        {
            std::unique_lock<std::mutex> lock(g_queue_mutex);
            g_queue_cv.wait(lock, [](){ return !g_running.load() || !g_queue.empty(); });
            if (!g_running.load()) break;
            text = g_queue.front();
            g_queue.pop_front();
        }
        g_busy.store(true);
        process_text(text, 0);
        g_busy.store(false);
    }
}

std::string clean_text(const std::string& input)
{
    std::string out;
    out.reserve(std::min((size_t)2000, input.size()));
    for (size_t i = 0; i < input.size() && out.size() < 2000; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c < 0x20 && c != '\t') continue;
        out.push_back((char)c);
    }
    size_t first = out.find_first_not_of(" \t");
    size_t last = out.find_last_not_of(" \t");
    if (first == std::string::npos) return std::string();
    return out.substr(first, last - first + 1);
}

} // namespace

bool ScreenTTS_Init()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) return true;
    try {
        g_worker = std::thread(worker_main);
    } catch (...) {
        g_running.store(false);
        return false;
    }
    printf("[ScreenTTS] 已启动：TTSMaker在线模型，屏幕喇叭独立队列（默认voice=1504）\n");
    return true;
}

void ScreenTTS_Shutdown()
{
    if (!g_running.exchange(false)) return;
    g_queue_cv.notify_all();
    if (g_worker.joinable()) g_worker.join();
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    g_queue.clear();
    g_busy.store(false);
}

bool ScreenTTS_Speak(const std::string& utf8_text)
{
    if (!g_running.load()) return false;
    std::string text = clean_text(utf8_text);
    if (text.empty()) return false;
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    if (g_queue.size() >= kQueueLimit) return false;
    g_queue.push_back(text);
    g_queue_cv.notify_one();
    return true;
}

bool ScreenTTS_Busy()
{
    if (g_busy.load()) return true;
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    return !g_queue.empty();
}
