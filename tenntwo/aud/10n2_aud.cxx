/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/types.h>

#include <arch/board/board.h>
#include <arch/chip/audio.h>
#include <nuttx/arch.h>
#include <10n2_aud.h>
#include <fcntl.h>

static bool running = true;
#define AUD_QUEUE_NAME "/aud_queue2" /* Queue name. */
#define AUD_QUEUE_PERMS ((int)(0644))
#define AUD_QUEUE_MAXMSG 64               /* Maximum number of messages. */
#define AUD_QUEUE_MSGSIZE sizeof(aud_req) /* Length of message. */
#define AUD_QUEUE_ATTR_INITIALIZER ((struct mq_attr){AUD_QUEUE_MAXMSG, AUD_QUEUE_MSGSIZE, 0, 0})

#define AUD_QUEUE_POLL ((struct timespec){0, 100000000})

static struct mq_attr attr_mq = AUD_QUEUE_ATTR_INITIALIZER;
static pthread_t th_consumer;

void *_q_read(void *args)
{
  (void)args; /* Suppress -Wunused-parameter warning. */
  /* Initialize the queue attributes */
  int cpu = up_cpu_index();
  printf("aud CPU %d\n", cpu);

  sigset_t mask;
  sigemptyset(&mask);
  // TOOD 18
  sigaddset(&mask, 18);
  int ret = sigprocmask(SIG_BLOCK, &mask, NULL);
  if (ret != OK)
  {
    printf("TF ERROR sigprocmask failed. %d\n", ret);
  }
  /* Create the message queue. The queue reader is NONBLOCK. */
  mqd_t r_mq = mq_open(AUD_QUEUE_NAME, O_CREAT | O_RDWR | O_NONBLOCK, AUD_QUEUE_PERMS, &attr_mq);

  if (r_mq < 0)
  {
    fprintf(stderr, "[CONSUMER]: Error, cannot open the queue: %s.\n", strerror(errno));
    return NULL;
  }

  printf("[CONSUMER]: Queue opened, queue descriptor: %d.\n", r_mq);

  unsigned int prio;
  ssize_t bytes_read;
  char buffer[AUD_QUEUE_MSGSIZE];
  struct timespec poll_sleep;

  while (running)
  {
    memset(buffer, 0x00, sizeof(buffer));
    bytes_read = mq_receive(r_mq, buffer, AUD_QUEUE_MSGSIZE, &prio);
    aud_req *r = (aud_req *)buffer;
    if (bytes_read >= 0)
    {
      aud_beep(r->en, r->vol, r->freq);
      struct timespec aud_sleep
      {
        r->dur / 1000, (r->dur % 1000) * 1e6
      };
      nanosleep(&aud_sleep, NULL);
      // off
      aud_beep(0, 255, 0);
    }
    else
    {
      poll_sleep = AUD_QUEUE_POLL;
      nanosleep(&poll_sleep, NULL);
    }

    fflush(stdout);
  }
  printf("aud cleaning mq\n");
  mq_close(r_mq);
  // mq_unlink(QUEUE_NAME);
  return NULL;
}

bool send_aud_seq(aud_req *req, uint8_t len)
{

  for (int i = 0; i < len; i++)
  {
    send_aud_beep(req[i].en, req[i].vol, req[i].freq, req[i].dur);
  }

  return true;
}
bool send_aud_beep(bool en, int16_t vol, uint16_t freq, uint32_t dur)
{
  aud_req r = {en, vol, freq, dur};
  mqd_t mq = mq_open(AUD_QUEUE_NAME, O_WRONLY | O_NONBLOCK);
  if (mq < 0)
  {
    fprintf(stderr, "[aud sender]: Error, cannot open the queue: %s.\n", strerror(errno));
    return false;
  }
  if (mq_send(mq, (char *)&r, AUD_QUEUE_MSGSIZE, 1) < 0)
  {
    fprintf(stderr, "[aud sender]: Error, cannot send: %i, %s.\n", errno, strerror(errno));
  }

  return true;
}

bool aud_init(void)
{
  running = true;

  printf("init audio\n");

  /* Enable I2S pin. */
  cxd56_audio_en_i2s_io();

  /* Enable speaker output. */
  cxd56_audio_set_spout(true);

  /* Power on Audio driver */
  if (cxd56_audio_poweron() != CXD56_AUDIO_ECODE_OK)
  {
    return false;
  }

  /* Enable BaseBand driver output */
  if (cxd56_audio_en_output() != CXD56_AUDIO_ECODE_OK)
  {
    return false;
  }

  if (board_external_amp_mute_control(false) != OK)
  {
    return false;
  }

  cpu_set_t cpuset = 1 << 3;
  int rc = pthread_setaffinity_np(th_consumer, sizeof(cpu_set_t), &cpuset);
  pthread_create(&th_consumer, NULL, &_q_read, NULL);
  if (rc != 0)
  {
    printf("Unable set CPU affinity : %d", rc);
  }

  // off
  aud_beep(0, 255, 0);
  printf("done init audio\n");
  return true;
}

bool aud_teardown(void)
{
  printf("tearing down audio\n");
  /* Disable speaker output. */
  cxd56_audio_set_spout(false);

  /* Disable I2S pin. */

  cxd56_audio_dis_i2s_io();

  /* Power off Audio driver */

  if (cxd56_audio_dis_output() != CXD56_AUDIO_ECODE_OK)
  {
    return false;
  }

  /* Disable BaseBand driver output */

  if (cxd56_audio_poweroff() != CXD56_AUDIO_ECODE_OK)
  {
    return false;
  }

  running = false;
  pthread_join(th_consumer, NULL);
  return true;
}

bool aud_beep(bool en, int16_t vol, uint16_t freq)
{
  if (!en)
  {
    /* Stop beep */

    if (cxd56_audio_stop_beep() != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }
  }

  if (0 != freq)
  {
    /* Set beep frequency parameter */

    if (cxd56_audio_set_beep_freq(freq) != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }
  }

  if (255 != vol)
  {
    /* Set beep volume parameter */

    if (cxd56_audio_set_beep_vol(vol) != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }
  }

  if (en)
  {
    /* Play beep */

    if (cxd56_audio_play_beep() != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }
  }

  return true;
}
