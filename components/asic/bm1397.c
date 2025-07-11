#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "serial.h"
#include "bm1397.h"
#include "utils.h"
#include "crc.h"
#include "mining.h"
#include "global_state.h"

#define BM1397_CHIP_ID 0x1397
#define BM1397_CHIP_ID_RESPONSE_LENGTH 9

#define TYPE_JOB 0x20
#define TYPE_CMD 0x40

#define GROUP_SINGLE 0x00
#define GROUP_ALL 0x10

#define CMD_JOB 0x01

#define CMD_SETADDRESS 0x00
#define CMD_WRITE 0x01
#define CMD_READ 0x02
#define CMD_INACTIVE 0x03

#define RESPONSE_CMD 0x00
#define RESPONSE_JOB 0x80

#define SLEEP_TIME 20
#define FREQ_MULT 25.0

#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define TICKET_MASK 0x14
#define MISC_CONTROL 0x18

typedef struct __attribute__((__packed__))
{
    uint16_t preamble;
    uint32_t nonce;
    uint8_t midstate_num;
    uint8_t job_id;
    uint8_t crc;
} bm1397_asic_result_t;

static const char * TAG = "bm1397";

static uint32_t prev_nonce = 0;
static task_result result;

/// @brief
/// @param ftdi
/// @param header
/// @param data
/// @param len
static void _send_BM1397(uint8_t header, uint8_t *data, uint8_t data_len, bool debug)
{
    packet_type_t packet_type = (header & TYPE_JOB) ? JOB_PACKET : CMD_PACKET;
    uint8_t total_length = (packet_type == JOB_PACKET) ? (data_len + 6) : (data_len + 5);

    // allocate memory for buffer
    unsigned char *buf = malloc(total_length);

    // add the preamble
    buf[0] = 0x55;
    buf[1] = 0xAA;

    // add the header field
    buf[2] = header;

    // add the length field
    buf[3] = (packet_type == JOB_PACKET) ? (data_len + 4) : (data_len + 3);

    // add the data
    memcpy(buf + 4, data, data_len);

    // add the correct crc type
    if (packet_type == JOB_PACKET)
    {
        uint16_t crc16_total = crc16_false(buf + 2, data_len + 2);
        buf[4 + data_len] = (crc16_total >> 8) & 0xFF;
        buf[5 + data_len] = crc16_total & 0xFF;
    }
    else
    {
        buf[4 + data_len] = crc5(buf + 2, data_len + 2);
    }

    // send serial data
    SERIAL_send(buf, total_length, debug);

    free(buf);
}

static void _send_read_address(void)
{
    unsigned char read_address[2] = {0x00, 0x00};
    // send serial data
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_READ), read_address, 2, BM1397_SERIALTX_DEBUG);
}

static void _send_chain_inactive(void)
{

    unsigned char read_address[2] = {0x00, 0x00};
    // send serial data
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_INACTIVE), read_address, 2, BM1397_SERIALTX_DEBUG);
}

static void _set_chip_address(uint8_t chipAddr)
{

    unsigned char read_address[2] = {chipAddr, 0x00};
    // send serial data
    _send_BM1397((TYPE_CMD | GROUP_SINGLE | CMD_SETADDRESS), read_address, 2, BM1397_SERIALTX_DEBUG);
}

void BM1397_set_version_mask(uint32_t version_mask) {
    // placeholder
}

// borrowed from cgminer driver-gekko.c calc_gsf_freq()
void BM1397_send_hash_frequency(float frequency)
{
    unsigned char prefreq1[9] = {0x00, 0x70, 0x0F, 0x0F, 0x0F, 0x00}; // prefreq - pll0_divider

    // default 200Mhz if it fails
    unsigned char freqbuf[9] = {0x00, 0x08, 0x40, 0xA0, 0x02, 0x25}; // freqbuf - pll0_parameter

    float deffreq = 200.0;

    float fa, fb, fc1, fc2, newf;
    float f1, basef, famax = 0x104, famin = 0x10;
    int i;

    // bound the frequency setting
    //  You can go as low as 13 but it doesn't really scale or
    //  produce any nonces
    if (frequency < 50)
    {
        f1 = 50;
    }
    else if (frequency > 650)
    {
        f1 = 650;
    }
    else
    {
        f1 = frequency;
    }

    fb = 2;
    fc1 = 1;
    fc2 = 5; // initial multiplier of 10
    if (f1 >= 500)
    {
        // halve down to '250-400'
        fb = 1;
    }
    else if (f1 <= 150)
    {
        // triple up to '300-450'
        fc1 = 3;
    }
    else if (f1 <= 250)
    {
        // double up to '300-500'
        fc1 = 2;
    }
    // else f1 is 250-500

    // f1 * fb * fc1 * fc2 is between 2500 and 6500
    // - so round up to the next 25 (freq_mult)
    basef = FREQ_MULT * ceil(f1 * fb * fc1 * fc2 / FREQ_MULT);

    // fa should be between 100 (0x64) and 200 (0xC8)
    fa = basef / FREQ_MULT;

    // code failure ... basef isn't 400 to 6000
    if (fa < famin || fa > famax)
    {
        newf = deffreq;
    }
    else
    {
        freqbuf[2] = 0x40 + (unsigned char)((int)fa >> 8);
        freqbuf[3] = (unsigned char)((int)fa & 0xff);
        freqbuf[4] = (unsigned char)fb;
        // fc1, fc2 'should' already be 1..15
        freqbuf[5] = (((unsigned char)fc1 & 0x7) << 4) + ((unsigned char)fc2 & 0x7);
        
        newf = basef / ((float)fb * (float)fc1 * (float)fc2);
    }

    for (i = 0; i < 2; i++)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), prefreq1, 6, BM1397_SERIALTX_DEBUG);
    }
    for (i = 0; i < 2; i++)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), freqbuf, 6, BM1397_SERIALTX_DEBUG);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Setting Frequency to %.2fMHz (%.2f)", frequency, newf);
}

uint8_t BM1397_init(uint64_t frequency, uint16_t asic_count, uint16_t difficulty)
{
    // send the init command
    _send_read_address();

    int chip_counter = count_asic_chips(asic_count, BM1397_CHIP_ID, BM1397_CHIP_ID_RESPONSE_LENGTH);

    if (chip_counter == 0) {
        return 0;
    }

    // send serial data
    vTaskDelay(SLEEP_TIME / portTICK_PERIOD_MS);
    _send_chain_inactive();

    // split the chip address space evenly
    for (uint8_t i = 0; i < asic_count; i++) {
        _set_chip_address(i * (256 / asic_count));
    }

    unsigned char init[6] = {0x00, CLOCK_ORDER_CONTROL_0, 0x00, 0x00, 0x00, 0x00}; // init1 - clock_order_control0
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init, 6, BM1397_SERIALTX_DEBUG);

    unsigned char init2[6] = {0x00, CLOCK_ORDER_CONTROL_1, 0x00, 0x00, 0x00, 0x00}; // init2 - clock_order_control1
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init2, 6, BM1397_SERIALTX_DEBUG);

    unsigned char init3[9] = {0x00, ORDERED_CLOCK_ENABLE, 0x00, 0x00, 0x00, 0x01}; // init3 - ordered_clock_enable
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init3, 6, BM1397_SERIALTX_DEBUG);

    unsigned char init4[9] = {0x00, CORE_REGISTER_CONTROL, 0x80, 0x00, 0x80, 0x74}; // init4 - init_4_?
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init4, 6, BM1397_SERIALTX_DEBUG);

    BM1397_set_job_difficulty_mask(difficulty);

    unsigned char init5[9] = {0x00, PLL3_PARAMETER, 0xC0, 0x70, 0x01, 0x11}; // init5 - pll3_parameter
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init5, 6, BM1397_SERIALTX_DEBUG);

    unsigned char init6[9] = {0x00, FAST_UART_CONFIGURATION, 0x06, 0x00, 0x00, 0x0F}; // init6 - fast_uart_configuration
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), init6, 6, BM1397_SERIALTX_DEBUG);

    BM1397_set_default_baud();

    BM1397_send_hash_frequency(frequency);

    return chip_counter;
}

// Baud formula = 25M/((denominator+1)*8)
// The denominator is 5 bits found in the misc_control (bits 9-13)
int BM1397_set_default_baud(void)
{
    // default divider of 26 (11010) for 115,749
    unsigned char baudrate[9] = {0x00, MISC_CONTROL, 0x00, 0x00, 0b01111010, 0b00110001}; // baudrate - misc_control
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), baudrate, 6, BM1397_SERIALTX_DEBUG);
    return 115749;
}

int BM1397_set_max_baud(void)
{
    // divider of 0 for 3,125,000
    ESP_LOGI(TAG, "Setting max baud of 3125000");
    unsigned char baudrate[9] = {0x00, MISC_CONTROL, 0x00, 0x00, 0b01100000, 0b00110001};
    ; // baudrate - misc_control
    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), baudrate, 6, BM1397_SERIALTX_DEBUG);
    return 3125000;
}

void BM1397_set_job_difficulty_mask(int difficulty)
{

    // Default mask of 256 diff
    unsigned char job_difficulty_mask[9] = {0x00, TICKET_MASK, 0b00000000, 0b00000000, 0b00000000, 0b11111111};

    // The mask must be a power of 2 so there are no holes
    // Correct:  {0b00000000, 0b00000000, 0b11111111, 0b11111111}
    // Incorrect: {0b00000000, 0b00000000, 0b11100111, 0b11111111}
    difficulty = _largest_power_of_two(difficulty) - 1; // (difficulty - 1) if it is a pow 2 then step down to second largest for more hashrate sampling

    // convert difficulty into char array
    // Ex: 256 = {0b00000000, 0b00000000, 0b00000000, 0b11111111}, {0x00, 0x00, 0x00, 0xff}
    // Ex: 512 = {0b00000000, 0b00000000, 0b00000001, 0b11111111}, {0x00, 0x00, 0x01, 0xff}
    for (int i = 0; i < 4; i++)
    {
        char value = (difficulty >> (8 * i)) & 0xFF;
        // The char is read in backwards to the register so we need to reverse them
        // So a mask of 512 looks like 0b00000000 00000000 00000001 1111111
        // and not 0b00000000 00000000 10000000 1111111

        job_difficulty_mask[5 - i] = _reverse_bits(value);
    }

    ESP_LOGI(TAG, "Setting job ASIC mask to %d", difficulty);

    _send_BM1397((TYPE_CMD | GROUP_ALL | CMD_WRITE), job_difficulty_mask, 6, BM1397_SERIALTX_DEBUG);
}

static uint8_t id = 0;

void BM1397_send_work(void *pvParameters, bm_job *next_bm_job)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    job_packet job;
    // max job number is 128
    // there is still some really weird logic with the job id bits for the asic to sort out
    // so we have it limited to 128 and it has to increment by 4
    id = (id + 4) % 128;

    job.job_id = id;
    job.num_midstates = next_bm_job->num_midstates;
    memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
    memcpy(&job.nbits, &next_bm_job->target, 4);
    memcpy(&job.ntime, &next_bm_job->ntime, 4);
    memcpy(&job.merkle4, next_bm_job->merkle_root + 28, 4);
    memcpy(job.midstate, next_bm_job->midstate, 32);

    if (job.num_midstates == 4)
    {
        memcpy(job.midstate1, next_bm_job->midstate1, 32);
        memcpy(job.midstate2, next_bm_job->midstate2, 32);
        memcpy(job.midstate3, next_bm_job->midstate3, 32);
    }

    if (GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id] != NULL)
    {
        free_bm_job(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id]);
    }

    GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job.job_id] = next_bm_job;

    pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock);
    GLOBAL_STATE->valid_jobs[job.job_id] = 1;
    pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);

    #if BM1397_DEBUG_JOBS
    ESP_LOGI(TAG, "Send Job: %02X", job.job_id);
    #endif

    _send_BM1397((TYPE_JOB | GROUP_SINGLE | CMD_WRITE), (uint8_t *)&job, sizeof(job_packet), BM1397_DEBUG_WORK);
}

task_result *BM1397_process_work(void *pvParameters)
{
    bm1397_asic_result_t asic_result = {0};

    if (receive_work((uint8_t *)&asic_result, sizeof(asic_result)) == ESP_FAIL) {
        return NULL;
    }

    uint8_t nonce_found = 0;
    uint32_t first_nonce = 0;

    uint8_t rx_job_id = asic_result.job_id & 0xfc;
    uint8_t rx_midstate_index = asic_result.job_id & 0x03;

    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;
    if (GLOBAL_STATE->valid_jobs[rx_job_id] == 0)
    {
        ESP_LOGW(TAG, "Invalid job nonce found, id=%d", rx_job_id);
        return NULL;
    }

    uint32_t rolled_version = GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version;
    for (int i = 0; i < rx_midstate_index; i++)
    {
        rolled_version = increment_bitmask(rolled_version, GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[rx_job_id]->version_mask);
    }

    // ASIC may return the same nonce multiple times
    // or one that was already found
    // most of the time it behavies however
    if (nonce_found == 0)
    {
        first_nonce = asic_result.nonce;
        nonce_found = 1;
    }
    else if (asic_result.nonce == first_nonce)
    {
        // stop if we've already seen this nonce
        return NULL;
    }

    if (asic_result.nonce == prev_nonce)
    {
        return NULL;
    }
    else
    {
        prev_nonce = asic_result.nonce;
    }

    result.job_id = rx_job_id;
    result.nonce = asic_result.nonce;
    result.rolled_version = rolled_version;

    return &result;
}
