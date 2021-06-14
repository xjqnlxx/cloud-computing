#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

/**
 * srTCM
 */

struct rte_meter_srtcm srtcm[APP_FLOWS_MAX];
struct rte_meter_srtcm_profile srtcm_profile[APP_FLOWS_MAX];
struct rte_meter_srtcm_params srtcm_params[] = {
	{.cir = 160000000,  .cbs = 80000, .ebs = 160000},
	{.cir = 80000000,  .cbs = 40000, .ebs = 80000},
	{.cir = 40000000,  .cbs = 20000, .ebs = 40000},
	{.cir = 20000000,  .cbs = 10000, .ebs = 20000},
};
struct rte_red_config red_config[RTE_COLORS]; 
struct rte_red queue[APP_FLOWS_MAX][RTE_COLORS]; 
uint32_t queue_size[APP_FLOWS_MAX][RTE_COLORS] = {0}; 
uint64_t pretime = 0; 

uint64_t hz;
uint64_t cycle;

int
qos_meter_init(void)
{
    cycle = rte_get_tsc_cycles();
    hz = rte_get_tsc_hz();
    for (int i = 0; i < APP_FLOWS_MAX; i++) {
        if (rte_meter_srtcm_profile_config(&srtcm_profile[i], &srtcm_params[i])) 
            rte_panic("Cannot init srTCM profile\n"); 
        if (rte_meter_srtcm_config(&srtcm[i], &srtcm_profile[i])) 
            rte_panic("Cannot init srTCM\n"); 
    }
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    time = time * hz / 1000000000 + cycle;
    return rte_meter_srtcm_color_blind_check(&srtcm[flow_id], &srtcm_profile[flow_id], time, pkt_len);
}


/**
 * WRED
 */

int
qos_dropper_init(void)
{
    if (rte_red_config_init(&red_config[GREEN], RTE_RED_WQ_LOG2_MIN, 1022, 1023, 10)) 
        rte_panic("Cannot init GREEN config\n"); 
    if (rte_red_config_init(&red_config[YELLOW], RTE_RED_WQ_LOG2_MIN, 1022, 1023, 10)) 
        rte_panic("Cannot init YELLOW config\n"); 
    if (rte_red_config_init(&red_config[RED], RTE_RED_WQ_LOG2_MIN, 0, 1, 10)) 
        rte_panic("Cannot init RED config\n"); 
    for (int i = 0; i < APP_FLOWS_MAX; ++i) {
        for (int j = 0; j < RTE_COLORS; ++j) {
            if (rte_red_rt_data_init(&queue[i][j])) 
                rte_panic("Cannot init queue %d %d\n", i, j);
        }
    }
    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    if (time != pretime) { 
        memset(queue_size, 0, sizeof(queue_size));
        for (int i = 0; i < APP_FLOWS_MAX; ++i) {
            for (int j = 0; j < RTE_COLORS; ++j) {
                rte_red_mark_queue_empty(&queue[i][j], time);
            }
        }
    }
    pretime = time;

    int ret;
    if (!(ret = rte_red_enqueue(&red_config[color], &queue[flow_id][color], queue_size[flow_id][color], time))) {
        ++queue_size[flow_id][color];
        return 0;
    }
    return 1;
}