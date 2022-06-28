

#ifndef TENN2_AUD_H
#define TENN2_AUD_H

// Expose a C friendly interface for main functions.
#ifdef __cplusplus
extern "C" {
#endif

struct aud_req
{
  bool en;
  int16_t vol;
  uint16_t freq;
  uint32_t dur; // milli
};

#define STARTUP_JINGLE_LEN 5

static struct aud_req startup_jingle[STARTUP_JINGLE_LEN] = {
  {1,-40,400,100},
  {1,-40,500,100},
  {1,-40,600,100},
  {1,-40,600,100},
  {1,-40,300,100},
};

#define SHUTDOWN_JINGLE_LEN 5

static struct aud_req shutdown_jingle[SHUTDOWN_JINGLE_LEN] = {
  {1,-40,300,100},
  {1,-40,600,100},
  {1,-40,600,100},
  {1,-40,500,100},
  {1,-40,400,100},
};

#define GNSS_JINGLE_LEN 2

static struct aud_req gnss_jingle[GNSS_JINGLE_LEN] = {
  {1,-40,1400,100},
  {1,-40,1500,100},
};


#define BTN_ON_JINGLE_LEN 6

static struct aud_req btn_on_jingle[BTN_ON_JINGLE_LEN] = {
  {1,-40,1400,10},
  {0,255,0,0}, //off
  {1,-40,1600,0},
  {0,255,0,10}, //off
  {1,-40,1400,0},
  {0,255,0,10}, //off
};

bool aud_init(void);

bool aud_teardown(void);

bool aud_beep(bool en, int16_t vol , uint16_t freq );

bool send_aud_beep(bool en,int16_t vol, uint16_t freq,uint32_t dur);

bool send_aud_seq(struct aud_req* req,uint8_t len);

#ifdef __cplusplus
}
#endif

#endif  
