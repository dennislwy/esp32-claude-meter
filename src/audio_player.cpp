#include "audio_player.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "board_pins.h"
#include "es8311.h"
#include "settings.h"

namespace
{
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr size_t CHUNK_FRAMES = 256;
constexpr int DMA_BUF_COUNT = 4;
constexpr int DMA_BUF_FRAMES = 256;

// 0-100 percent → [-40, 0] dB. 0 % reads as effectively mute (-60 dB).
float volumeDb()
{
  const uint8_t pct = settings::audioVolume();
  if (pct == 0)
    return -60.0f;
  return -40.0f + pct * 40.0f / 100.0f;
}

struct WavInfo
{
  uint32_t sampleRate;
  uint16_t channels;
  uint32_t dataBytes;
};

// Walks the RIFF chunks, leaving the file positioned at the start of the sample data
bool readWavHeader(File &file, WavInfo &info)
{
  char id[4];
  uint32_t size;
  if (file.read((uint8_t *)id, 4) != 4 || memcmp(id, "RIFF", 4) != 0 ||
      file.read((uint8_t *)&size, 4) != 4 ||
      file.read((uint8_t *)id, 4) != 4 || memcmp(id, "WAVE", 4) != 0)
  {
    return false;
  }

  bool haveFormat = false;
  while (file.read((uint8_t *)id, 4) == 4 && file.read((uint8_t *)&size, 4) == 4)
  {
    if (memcmp(id, "fmt ", 4) == 0)
    {
      uint8_t fmt[16];
      if (size < sizeof(fmt) || file.read(fmt, sizeof(fmt)) != sizeof(fmt))
      {
        return false;
      }
      uint16_t format;
      uint16_t bits;
      memcpy(&format, fmt, 2);
      memcpy(&info.channels, fmt + 2, 2);
      memcpy(&info.sampleRate, fmt + 4, 4);
      memcpy(&bits, fmt + 14, 2);
      if (format != 1 || bits != 16 || info.channels < 1 || info.channels > 2)
      {
        return false;
      }
      file.seek(size - sizeof(fmt) + (size & 1), SeekCur);
      haveFormat = true;
    }
    else if (memcmp(id, "data", 4) == 0)
    {
      info.dataBytes = size;
      return haveFormat;
    }
    else
    {
      // Chunks are padded to an even size
      file.seek(size + (size & 1), SeekCur);
    }
  }
  return false;
}

bool startI2s(uint32_t sampleRate)
{
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = sampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = DMA_BUF_COUNT;
  config.dma_buf_len = DMA_BUF_FRAMES;
  config.tx_desc_auto_clear = true; // send silence if we fall behind
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(I2S_PORT, &config, 0, nullptr) != ESP_OK)
  {
    return false;
  }
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK)
  {
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }
  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

// Sends the sample data as stereo frames; mono samples go to both channels
void streamSamples(File &file, const WavInfo &info)
{
  int16_t in[CHUNK_FRAMES * 2];
  int16_t out[CHUNK_FRAMES * 2];
  const size_t frameBytes = info.channels * sizeof(int16_t);
  uint32_t remaining = info.dataBytes;
  while (remaining >= frameBytes)
  {
    const size_t want = min<uint32_t>(remaining, CHUNK_FRAMES * frameBytes);
    const size_t got = file.read((uint8_t *)in, want);
    const size_t frames = got / frameBytes;
    if (frames == 0)
    {
      break;
    }
    for (size_t i = 0; i < frames; i++)
    {
      out[2 * i] = in[i * info.channels];
      out[2 * i + 1] = in[i * info.channels + info.channels - 1];
    }
    size_t written;
    i2s_write(I2S_PORT, out, frames * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    remaining -= frames * frameBytes;
  }
}
}

bool playWav(const char *path)
{
  if (!LittleFS.begin(false))
  {
    Serial.println("LittleFS mount failed - run: pio run -t uploadfs");
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file)
  {
    Serial.printf("%s not found\n", path);
    return false;
  }
  WavInfo info;
  if (!readWavHeader(file, info))
  {
    Serial.printf("%s is not a 16-bit PCM WAV\n", path);
    return false;
  }

  // The codec needs MCLK running before it is configured
  if (!startI2s(info.sampleRate))
  {
    Serial.println("I2S start failed");
    return false;
  }
  Es8311 codec;
  if (!codec.begin())
  {
    Serial.println("ES8311 not responding");
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }
  codec.start(info.sampleRate);
  codec.setVolume(volumeDb());

  // Amplifier only on while playing, to avoid hiss and save power
  pinMode(PIN_SPEAKER_AMP, OUTPUT);
  digitalWrite(PIN_SPEAKER_AMP, HIGH);

  const uint32_t start = millis();
  streamSamples(file, info);
  // Let the queued DMA buffers play out before muting
  delay(DMA_BUF_COUNT * DMA_BUF_FRAMES * 1000 / info.sampleRate + 20);

  digitalWrite(PIN_SPEAKER_AMP, LOW);
  codec.stop();
  i2s_driver_uninstall(I2S_PORT);
  Serial.printf("Played %s: %u Hz, %u ch, %u ms\n", path, info.sampleRate, info.channels, millis() - start);
  return true;
}
