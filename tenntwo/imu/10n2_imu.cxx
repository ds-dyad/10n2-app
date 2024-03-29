
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
#include <nuttx/arch.h>
#include <sys/stat.h>
#include <imu.h>
#include <10n2_imu.h>

static bool imu_running = true;
#define IMU_QUEUE_NAME "/imu_queue" /* Queue name. */
#define IMU_QUEUE_PERMS ((int)(0644))
#define IMU_QUEUE_MAXMSG 16               /* Maximum number of messages. */
#define IMU_QUEUE_MSGSIZE sizeof(imu_req) /* Length of message. */
#define IMU_QUEUE_ATTR_INITIALIZER ((struct mq_attr){IMU_QUEUE_MAXMSG, IMU_QUEUE_MSGSIZE, 0, 0})


static struct mq_attr imu_attr_mq = IMU_QUEUE_ATTR_INITIALIZER;
static pthread_t imu_th_consumer;

float imu_ac_buf[2][6][IMU_BUF_SIZE]; // ping pong buffer for imu samples

int8_t current_imu_buffer = 0;
uint8_t next_imu_sample_idx = 0;

#define IMU_DEVNAME "/dev/imu0"

struct vel_gyro_s current_pmu;

float *get_latest_imu_samples(uint8_t sample_type)
{
    if ((next_imu_sample_idx - IMU_SAMPLE_SIZE) < 0)
        return nullptr;
    return &imu_ac_buf[current_imu_buffer][sample_type][next_imu_sample_idx - IMU_SAMPLE_SIZE];
}
void write_imu_sample(const vel_gyro_s &s, uint8_t ping_buf_idx, uint8_t sample_buf_idx)
{
    imu_ac_buf[ping_buf_idx][0][sample_buf_idx] = float(s.ac_x);
    imu_ac_buf[ping_buf_idx][1][sample_buf_idx] = float(s.ac_y);
    imu_ac_buf[ping_buf_idx][2][sample_buf_idx] = float(s.ac_z);
    imu_ac_buf[ping_buf_idx][3][sample_buf_idx] = float(s.gy_x);
    imu_ac_buf[ping_buf_idx][4][sample_buf_idx] = float(s.gy_y);
    imu_ac_buf[ping_buf_idx][5][sample_buf_idx] = float(s.gy_z);
}

void record_imu_sample(const vel_gyro_s &s)
{
    if (next_imu_sample_idx >= IMU_BUF_SIZE)
    {
        // toggle buffers
        current_imu_buffer = (current_imu_buffer + 1) % 2;
        write_imu_sample(s, current_imu_buffer, IMU_SAMPLE_SIZE - 1);
        next_imu_sample_idx = IMU_SAMPLE_SIZE;
    }
    else if (next_imu_sample_idx >= (IMU_BUF_SIZE - IMU_SAMPLE_SIZE) + 1)
    {
        // dual write
        write_imu_sample(s, current_imu_buffer, next_imu_sample_idx);
        write_imu_sample(s, (current_imu_buffer + 1) % 2, (next_imu_sample_idx - IMU_BUF_SIZE + IMU_SAMPLE_SIZE - 1));
        next_imu_sample_idx++;
    }
    else
    {
        // just write to current
        write_imu_sample(s, current_imu_buffer, next_imu_sample_idx);
        next_imu_sample_idx++;
    }
}

void *_imu_q_read(void *args)
{
    (void)args; /* Suppress -Wunused-parameter warning. */
    /* Initialize the queue attributes */

    //int cpu = up_cpu_index();
    //printf("IMU CPU %d\n", cpu);

    /* Create the message queue. The queue reader is NONBLOCK. */
    mqd_t r_mq = mq_open(IMU_QUEUE_NAME, O_CREAT | O_RDWR | O_NONBLOCK, IMU_QUEUE_PERMS, &imu_attr_mq);
    char imu_buf[sizeof(struct vel_gyro_s)];

    // Open IMU file
    int imu_fd;
    if (r_mq < 0)
    {
        fprintf(stderr, "[IMU CONSUMER]: Error, cannot open the queue: %s.\n", strerror(errno));
        return NULL;
    }

    printf("[IMU CONSUMER]: Queue opened, queue descriptor: %d.\n", r_mq);
    imu_fd = open(IMU_DEVNAME, O_RDONLY);

    unsigned int prio;
    ssize_t bytes_read;
    char buffer[IMU_QUEUE_MSGSIZE];
    struct timespec poll_sleep;

    while (imu_running)
    {
        bytes_read = mq_receive(r_mq, buffer, IMU_QUEUE_MSGSIZE, &prio);
        imu_req *r = (imu_req *)buffer;
        if (bytes_read >= 0)
        {
            for (int i = 0; i < r->num && imu_running; i++)
            {
                read(imu_fd, &imu_buf, sizeof(struct vel_gyro_s));
                usleep(r->delay * 1e3);
                current_pmu = get_mpu_data((int16_t *)imu_buf);
                record_imu_sample(current_pmu);
                // dump_data(current_pmu);
            }
        }
        else
        {
            usleep(100 * 1e3);
        }

        fflush(stdout);
    }
    printf("imu cleaning mq\n");
    mq_close(r_mq);
    return NULL;
}

bool send_imu_req(struct imu_req req)
{
    mqd_t mq = mq_open(IMU_QUEUE_NAME, O_WRONLY | O_NONBLOCK);
    if (mq < 0)
    {
        fprintf(stderr, "[sender]: Error, cannot open the queue: %s.\n", strerror(errno));
        return false;
    }
    if (mq_send(mq, (char *)&req, IMU_QUEUE_MSGSIZE, 1) < 0)
    {
        fprintf(stderr, "[sender]: Error, cannot send: %i, %s.\n", errno, strerror(errno));
    }

    mq_close(mq);
    return true;
}

bool imu_init(void)
{
    printf("imu init\n");
    imu_running = true;
    cpu_set_t cpuset = 1 << 2;
    pthread_create(&imu_th_consumer, NULL, &_imu_q_read, NULL);
    int rc;
    rc = pthread_setaffinity_np(imu_th_consumer, sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        printf("Unable set CPU affinity : %d", rc);
    }

    return true;
}

bool imu_teardown(void)
{
    printf("imu teardown\n");
    imu_running = false;
    pthread_join(imu_th_consumer, NULL);
    return true;
}
