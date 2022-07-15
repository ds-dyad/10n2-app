
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

#include <imu.h>
#include <nuttx/clock.h>
#include <10n2_menu_handler.h>
#include <10n2_imu.h>
#include <10n2_aud.h>
#include <10n2_btn.h>
#include <10n2_cam.h>
#include <10n2_gnss.h>
#include <10n2_tf_pi.h>

static bool menu_handler_running = true;

static pthread_t menu_handler_th;

struct cam_req cam_hands_bw_r = {2, 0, 192, 12, 288, 108, 0, "hds"};
struct cam_req cam_cell_bw_r = {2, 0, 192, 12, 288, 108, 0, "cell"};
struct cam_req cam_none_bw_r = {2, 0, 192, 12, 288, 108, 0, "none"};
struct cam_req cam_nowrite_bw_r = {1, 0, 192, 12, 288, 108, 0, ""};
struct tf_req inf_r = {1, 0};

#define CAM_PERIOD 25
#define INF_PERIOD 25
#define POS_PERIOD 1
FILE *pos_pf = NULL;

#define POS_SAVE_DIR "/mnt/sd0/pos"

void close_pos_fd()
{
    if (pos_pf != NULL)
    {
        if (fclose(pos_pf) != 0)
        {
            printf("Unable to close pos file! :%s\n", strerror(errno));
        }
        else
        {
            printf("success!  closed pos output file\n");
        }
        pos_pf = NULL;
    }
}
void open_pos_fd()
{
    unsigned curr_time = clock();
    char namebuf[128];
    snprintf(namebuf, 128, "%s/imu-data-%i-%i.%s", POS_SAVE_DIR, current_submenu, curr_time, "csv");
    printf("opening :%s\n", namebuf);
    pos_pf = fopen(namebuf, "wb+");
    if (pos_pf == NULL)
    {
        printf("Unable to open pos! :%s\n", strerror(errno));
    }
    else
    {
        printf("success!  opened pos output file\n");
    }
    // TODO
    if (current_submenu == imu)
        fprintf(pos_pf, "t,acx,acy,acz,gyx,gyy,gyz\n");
    else if (current_submenu == gnss)
        fprintf(pos_pf, "t,y,M,d,h,m,s,us,t,lat,lon\n");
    else if (current_submenu == imu_gnss)
        fprintf(pos_pf, "t,acx,acy,acz,gyx,gyy,gyz,y,M,d,h,m,s,us,t,lat,lon\n");
}

void update_service(uint8_t last_submenu, uint32_t tick)
{
    if (current_menu == img)
    {
        if (current_submenu == cam_bw_on)
        {
            if ((tick % CAM_PERIOD) == 0)
                send_cam_req(cam_nowrite_bw_r);
        }
        if (current_submenu == cam_color_on)
        {
            //     if ((tick % CAM_PERIOD) == 0)
            // TODO       send_cam_req(cam_c_r);
        }
        else if (current_submenu == cam_hands_on)
        {
            if ((tick % CAM_PERIOD) == 0)
                send_cam_req(cam_hands_bw_r);
        }
        else if (current_submenu == cam_cell_on)
        {
            if ((tick % CAM_PERIOD) == 0)
                send_cam_req(cam_cell_bw_r);
        }
        else if (current_submenu == cam_none_on)
        {
            if ((tick % CAM_PERIOD) == 0)
                send_cam_req(cam_none_bw_r);
        }
    }
    else if (current_menu == pos)
    {
        if (current_submenu > 0 && last_submenu != current_submenu)
        {
            open_pos_fd();
        }
        if (current_submenu == 0 && last_submenu != current_submenu)
        {
            close_pos_fd();
        }

        unsigned curr_time = clock();
        if (current_submenu == imu)
        {
            fprintf(pos_pf, "%i,%i,%i,%i,%i,%i,%i\n",
                    curr_time,
                    current_pmu.ac_x,
                    current_pmu.ac_y,
                    current_pmu.ac_z,
                    current_pmu.gy_x,
                    current_pmu.gy_y,
                    current_pmu.gy_z);
        }
        else if (current_submenu == gnss)
        {
            fprintf(pos_pf, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%lf,%lf\n",
                    curr_time,
                    current_gnss.date.year,
                    current_gnss.date.month,
                    current_gnss.date.day,
                    current_gnss.time.hour,
                    current_gnss.time.minute,
                    current_gnss.time.sec,
                    current_gnss.time.usec,
                    current_gnss.type,
                    current_gnss.latitude,
                    current_gnss.longitude);
        }
        else if (current_submenu == imu_gnss)
        {
            fprintf(pos_pf, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%lf,%lf\n",
                    curr_time,
                    current_pmu.ac_x,
                    current_pmu.ac_y,
                    current_pmu.ac_z,
                    current_pmu.gy_x,
                    current_pmu.gy_y,
                    current_pmu.gy_z,
                    current_gnss.date.year,
                    current_gnss.date.month,
                    current_gnss.date.day,
                    current_gnss.time.hour,
                    current_gnss.time.minute,
                    current_gnss.time.sec,
                    current_gnss.time.usec,
                    current_gnss.type,
                    current_gnss.latitude,
                    current_gnss.longitude);
        }
    }
    else if (current_menu == inf)
    {
        if (current_submenu == inf_on)
        {
            if ((tick % INF_PERIOD) == 0)
            {
                send_cam_req(cam_nowrite_bw_r);
                send_tf_req(inf_r);
            }
        }
    }
}

void *_menu_run(void *args)
{
    (void)args; /* Suppress -Wunused-parameter warning. */
                /* Initialize the queue attributes */
    uint8_t last_menu = 0;
    uint8_t last_submenu = 0;

    struct timespec del_sleep
    {
        0, 1 * (long)1e6
    };

    uint32_t tick = 0;
    while (menu_handler_running)
    {
        update_service(last_submenu, tick++);
        last_menu = current_menu;
        last_submenu = current_submenu;
        nanosleep(&del_sleep, NULL);
    }
    return NULL;
}

bool menu_handler_init(void)
{
    printf("menu handler init\n");
    menu_handler_running = true;
    pthread_create(&menu_handler_th, NULL, &_menu_run, NULL);

    return true;
}

bool menu_handler_teardown(void)
{
    printf("menu handler teardown\n");
    menu_handler_running = false;
    pthread_join(menu_handler_th, NULL);
    return true;
}
