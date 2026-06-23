#include "midi_input.h"
#include <alsa/asoundlib.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <unistd.h>

struct AlsaMidiBackend {
  snd_seq_t* seq = nullptr;
  int port_id = -1;
  int source_client = -1;
  int source_port = -1;
  std::thread thread;
  MidiCallback* cb_ptr = nullptr;
  std::atomic<bool> running{false};
};

static void alsa_midi_thread_func(AlsaMidiBackend* b) {
  while (b->running.load(std::memory_order_acquire)) {
    snd_seq_event_t* ev = nullptr;
    int n = snd_seq_event_input(b->seq, &ev);
    if (n < 0) {
      if (n == -ENOSPC) {
        snd_seq_event_input(b->seq, &ev);
        continue;
      }
      if (n == -EAGAIN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      break;
    }

    switch (ev->type) {
    case SND_SEQ_EVENT_NOTEON: {
      uint8_t ch = ev->data.note.channel & 0x0F;
      uint8_t note = ev->data.note.note & 0x7F;
      uint8_t vel = ev->data.note.velocity & 0x7F;
      if (vel > 0) {
        (*b->cb_ptr)(0x90 | ch, note, vel);
      } else {
        (*b->cb_ptr)(0x80 | ch, note, 0);
      }
      break;
    }
    case SND_SEQ_EVENT_NOTEOFF: {
      uint8_t ch = ev->data.note.channel & 0x0F;
      uint8_t note = ev->data.note.note & 0x7F;
      uint8_t vel = ev->data.note.velocity & 0x7F;
      (*b->cb_ptr)(0x80 | ch, note, vel);
      break;
    }
    case SND_SEQ_EVENT_CONTROLLER: {
      uint8_t ch = ev->data.control.channel & 0x0F;
      uint8_t param = ev->data.control.param & 0x7F;
      uint8_t value = ev->data.control.value & 0x7F;
      (*b->cb_ptr)(0xB0 | ch, param, value);
      break;
    }
    case SND_SEQ_EVENT_PITCHBEND: {
      uint8_t ch = ev->data.control.channel & 0x0F;
      int bend = ev->data.control.value;
      uint8_t lsb = bend & 0x7F;
      uint8_t msb = (bend >> 7) & 0x7F;
      (*b->cb_ptr)(0xE0 | ch, lsb, msb);
      break;
    }
    case SND_SEQ_EVENT_START:
      (*b->cb_ptr)(0xFA, 0, 0);
      break;
    case SND_SEQ_EVENT_CONTINUE:
      (*b->cb_ptr)(0xFB, 0, 0);
      break;
    case SND_SEQ_EVENT_STOP:
      (*b->cb_ptr)(0xFC, 0, 0);
      break;
    default:
      break;
    }
  }
}

bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb) {
  auto* b = new AlsaMidiBackend;
  m->opaque = b;
  m->cb = nullptr;
  m->running = false;

  int err = snd_seq_open(&b->seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  if (err < 0) {
    fprintf(stderr, "MIDI: failed to open ALSA sequencer: %s\n", snd_strerror(err));
    delete b;
    m->opaque = nullptr;
    return false;
  }
  snd_seq_set_client_name(b->seq, "synth_front");

  b->port_id = snd_seq_create_simple_port(
      b->seq, "Input", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
  if (b->port_id < 0) {
    fprintf(stderr, "MIDI: failed to create ALSA port\n");
    snd_seq_close(b->seq);
    delete b;
    m->opaque = nullptr;
    return false;
  }

  auto* cb_ptr = new MidiCallback(std::move(cb));
  m->cb = cb_ptr;
  b->cb_ptr = cb_ptr;

  int my_client = snd_seq_client_id(b->seq);

  snd_seq_client_info_t* cinfo;
  snd_seq_port_info_t* pinfo;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_port_info_alloca(&pinfo);

  int found_client = -1;
  int found_port = -1;

  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(b->seq, cinfo) >= 0) {
    int c = snd_seq_client_info_get_client(cinfo);
    if (c == my_client)
      continue;

    snd_seq_port_info_set_client(pinfo, c);
    snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(b->seq, pinfo) >= 0) {
      unsigned int caps = snd_seq_port_info_get_capability(pinfo);
      unsigned int type = snd_seq_port_info_get_type(pinfo);
      if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ))
        continue;
      if (!(type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) && !(type & SND_SEQ_PORT_TYPE_SPECIFIC))
        continue;

      const char* name = snd_seq_port_info_get_name(pinfo);
      const char* client_name = snd_seq_client_info_get_name(cinfo);

      if (!source_name) {
        found_client = c;
        found_port = snd_seq_port_info_get_port(pinfo);
        goto done_searching;
      }

      char full[512];
      snprintf(full, sizeof(full), "%d:%d %s", c, snd_seq_port_info_get_port(pinfo), client_name);
      if (strstr(full, source_name) || (name && strstr(name, source_name))) {
        found_client = c;
        found_port = snd_seq_port_info_get_port(pinfo);
        goto done_searching;
      }
    }
  }
done_searching:

  if (found_client >= 0 && found_port >= 0) {
    err = snd_seq_connect_from(b->seq, b->port_id, found_client, found_port);
    if (err < 0) {
      fprintf(stderr, "MIDI: failed to connect to %d:%d: %s\n", found_client, found_port,
              snd_strerror(err));
      found_client = -1;
      found_port = -1;
    }
    b->source_client = found_client;
    b->source_port = found_port;
  } else if (source_name) {
    fprintf(stderr, "MIDI: no matching source \"%s\", running without MIDI input.\n", source_name);
  } else {
    fprintf(stderr, "MIDI: no MIDI input sources found, running without MIDI input.\n");
  }

  snd_seq_nonblock(b->seq, 1);

  b->running.store(true, std::memory_order_release);
  b->thread = std::thread(alsa_midi_thread_func, b);

  m->running = true;
  return true;
}

void midi_input_close(MidiInput* m) {
  auto* b = static_cast<AlsaMidiBackend*>(m->opaque);
  if (!b)
    return;

  b->running.store(false, std::memory_order_release);

  if (b->seq) {
    snd_seq_close(b->seq);
    b->seq = nullptr;
  }

  if (b->thread.joinable()) {
    b->thread.join();
  }

  if (m->cb) {
    delete m->cb;
    m->cb = nullptr;
  }
  delete b;
  m->opaque = nullptr;
  m->running = false;
}

std::vector<std::string> midi_input_list_sources() {
  std::vector<std::string> names;

  snd_seq_t* seq;
  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
    return names;

  int my_client = snd_seq_client_id(seq);

  snd_seq_client_info_t* cinfo;
  snd_seq_port_info_t* pinfo;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_port_info_alloca(&pinfo);

  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(seq, cinfo) >= 0) {
    int c = snd_seq_client_info_get_client(cinfo);
    if (c == my_client)
      continue;

    snd_seq_port_info_set_client(pinfo, c);
    snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(seq, pinfo) >= 0) {
      unsigned int caps = snd_seq_port_info_get_capability(pinfo);
      unsigned int type = snd_seq_port_info_get_type(pinfo);
      if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ))
        continue;
      if (!(type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) && !(type & SND_SEQ_PORT_TYPE_SPECIFIC))
        continue;

      char buf[512];
      snprintf(buf, sizeof(buf), "%d:%d %s", c, snd_seq_port_info_get_port(pinfo),
               snd_seq_client_info_get_name(cinfo));
      names.push_back(buf);
    }
  }

  snd_seq_close(seq);
  return names;
}
