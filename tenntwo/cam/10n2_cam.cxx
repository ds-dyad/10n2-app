
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <arch/board/board.h>
#include <arch/chip/audio.h>

#include <10n2_cam.h>
#include <image_provider.h>
#include <camera_fileutil.h>

static bool cam_running = true;
#define CAM_QUEUE_NAME "/cam_queue" /* Queue name. */
#define CAM_QUEUE_PERMS ((int)(0644))
#define CAM_QUEUE_MAXMSG 16               /* Maximum number of messages. */
#define CAM_QUEUE_MSGSIZE sizeof(cam_req) /* Length of message. */
#define CAM_QUEUE_ATTR_INITIALIZER ((struct mq_attr){CAM_QUEUE_MAXMSG, CAM_QUEUE_MSGSIZE, 0, 0})

#define CAM_QUEUE_POLL ((struct timespec){0, 100000000})

#define IMG_SAVE_DIR "/mnt/sd0"
static struct mq_attr cam_attr_mq = CAM_QUEUE_ATTR_INITIALIZER;
static pthread_t cam_th_consumer;

void *_cam_q_read(void *args)
{
    
    (void)args; /* Suppress -Wunused-parameter warning. */
    /* Initialize the queue attributes */

    /* Create the message queue. The queue reader is NONBLOCK. */
    mqd_t r_mq = mq_open(CAM_QUEUE_NAME, O_CREAT | O_RDWR | O_NONBLOCK, CAM_QUEUE_PERMS, &cam_attr_mq);

    if (r_mq < 0)
    {
        fprintf(stderr, "[CONSUMER]: Error, cannot open the queue: %s.\n", strerror(errno));
        return NULL;
    }

    printf("[CONSUMER]: Queue opened, queue descriptor: %d.\n", r_mq);

    unsigned int prio;
    ssize_t bytes_read;
    char buffer[CAM_QUEUE_MSGSIZE];
    char namebuf[128];
    struct timespec poll_sleep;
    // TODO make size configurable
    unsigned char *buf = (unsigned char *)memalign(32, 96 * 96);
    unsigned curr_time = (unsigned)time(NULL);

    while (cam_running)
    {
        memset(buffer, 0x00, sizeof(buffer));
        bytes_read = mq_receive(r_mq, buffer, CAM_QUEUE_MSGSIZE, &prio);
        cam_req *r = (cam_req *)buffer;
        if (bytes_read >= 0)
        {
            for (int i = 0; i < r->num; i++)
            {
                bzero(namebuf, 128);
                printf("getting image \n");
                spresense_getimage(buf);
                printf("init futil \n");
                futil_initialize();
                printf("writing image\n");
                snprintf(namebuf, 128, "%s/10n20-%i-%i.%s", IMG_SAVE_DIR, curr_time, i, "rgb");
                if (futil_writeimage(buf, 96 * 96, namebuf) < 0)
                {
                    printf("ERROR creating image \n");
                }
                struct timespec aud_sleep
                {
                    r->delay / 1000, (r->delay % 1000) * 1e6
                };
                nanosleep(&aud_sleep, NULL);
            }
        }
        else
        {
            poll_sleep = CAM_QUEUE_POLL;
            nanosleep(&poll_sleep, NULL);
        }

        fflush(stdout);
    }
    printf("cam cleaning mq\n");
    mq_close(r_mq);
    // mq_unlink(CAM_QUEUE_NAME);
    return NULL;
}

bool send_cam_req(struct cam_req req)
{
    mqd_t mq = mq_open(CAM_QUEUE_NAME, O_WRONLY);
    if (mq < 0)
    {
        fprintf(stderr, "[sender]: Error, cannot open the queue: %s.\n", strerror(errno));
        return false;
    }
    if (mq_send(mq, (char *)&req, CAM_QUEUE_MSGSIZE, 1) < 0)
    {
        fprintf(stderr, "[sender]: Error, cannot send: %i, %s.\n", errno, strerror(errno));
    }

    mq_close(mq);
    return true;
}

bool cam_init(void)
{
    printf("bws cam init\n");
    cam_running = true;
    pthread_create(&cam_th_consumer, NULL, &_cam_q_read, NULL);
    return true;
}

bool cam_teardown(void)
{
    printf("cam teardown\n");
    cam_running = false;
    pthread_join(cam_th_consumer, NULL);
    return true;
}
