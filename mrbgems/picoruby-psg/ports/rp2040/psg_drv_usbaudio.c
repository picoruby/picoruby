// #include "pico/stdlib.h"
// #include "tusb.h"
// 
// #include "../../include/psg.h"
// 
// #define USB_AUDIO_SAMPLE_RATE       22050
// #define USB_AUDIO_CHANNELS          2
// #define USB_AUDIO_RESOLUTION        16
// #define USB_AUDIO_BYTES_PER_SAMPLE  ((USB_AUDIO_RESOLUTION / 8) * USB_AUDIO_CHANNELS)
// #define USB_AUDIO_SAMPLES_PER_FRAME 22
// #define USB_AUDIO_BUFFER_SIZE       256
// 
// static struct {
//   uint16_t buffer[USB_AUDIO_BUFFER_SIZE * 2]; // stereo LRLR...
//   volatile uint32_t write_index;
// } usb_audio;
// 
// static void
// psg_usbaudio_init(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
// {
//   (void)a; (void)b; (void)c; (void)d;
//   usb_audio.write_index = 0;
// }
// 
// static void
// psg_usbaudio_start(void)
// {
//   // Flag on?
// }
// 
// static void
// psg_usbaudio_stop(void)
// {
//   // Flag off?
// }
// 
// static inline void
// psg_usbaudio_write(uint16_t l, uint16_t r)
// {
//   uint32_t i = usb_audio.write_index;
//   usb_audio.buffer[i % (USB_AUDIO_BUFFER_SIZE * 2)]     = l;
//   usb_audio.buffer[(i + 1) % (USB_AUDIO_BUFFER_SIZE * 2)] = r;
//   usb_audio.write_index = (i + 2) % (USB_AUDIO_BUFFER_SIZE * 2);
// }
// 
// const psg_output_api_t psg_drv_usbaudio = {
//   .init  = psg_usbaudio_init,
//   .start = psg_usbaudio_start,
//   .stop  = psg_usbaudio_stop,
//   .write = psg_usbaudio_write
// };
// 
// static uint32_t read_index = 0;
// 
// void
// audio_task(void)
// {
//   // 1ms data frame, 22 samples per frame
//   uint8_t frame_buf[USB_AUDIO_SAMPLES_PER_FRAME * USB_AUDIO_BYTES_PER_SAMPLE];
// 
//   for (uint32_t i = 0; i < USB_AUDIO_SAMPLES_PER_FRAME; ++i) {
//     uint16_t l12 = usb_audio.buffer[(read_index + 0) % (USB_AUDIO_BUFFER_SIZE * 2)];
//     uint16_t r12 = usb_audio.buffer[(read_index + 1) % (USB_AUDIO_BUFFER_SIZE * 2)];
//     read_index = (read_index + 2) % (USB_AUDIO_BUFFER_SIZE * 2);
// 
//     // Upsample 12-bit audio to 16-bit
//     uint16_t l16 = l12 << 4;
//     uint16_t r16 = r12 << 4;
// 
//     frame_buf[i * 4 + 0] = l16 & 0xFF;
//     frame_buf[i * 4 + 1] = l16 >> 8;
//     frame_buf[i * 4 + 2] = r16 & 0xFF;
//     frame_buf[i * 4 + 3] = r16 >> 8;
//   }
// 
//   tud_audio_write(frame_buf, sizeof(frame_buf));
// }
// 
