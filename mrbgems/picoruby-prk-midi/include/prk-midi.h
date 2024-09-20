#ifndef PRK_MIDI_DEFINED_H_
#define PRK_MIDI_DEFINED_H_

#include <stdint.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_WRITE_PACKET_SIZE 3

  void Midi_init(void);
  void Midi_stream_write(uint8_t *packet, uint8_t len);

#ifdef __cplusplus 
}
#endif

#endif /* PRK_MIDI_DEFINED_H_ */