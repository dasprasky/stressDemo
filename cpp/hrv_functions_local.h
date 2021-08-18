#ifndef _HRV_FUNCTIONS_LOCAL_H_
#define _HRV_FUNCTIONS_LOCAL_H_

#include <stdint.h>

#define SLIDING_WND                   (180)                   /*unit : second, 3 minutes*/
#define RAW_RECORD_LEN                (SLIDING_WND*3)
#define VALID_RECORD_LEN              (SLIDING_WND*3)
#define ANALYSIS_PERIOD               (10)
#define SEC_PER_MIN                   (60)
#define MS_PER_SEC                    (1000)
#define MAX_RR_DEVIATION              (300)                   /*unit : ms*/
#define MIN_CONTINUOUS_RR_CNT         (10)

#define RR_DIFF_WRONG_PERCENT         (33)
#define RR_DIFF_TOO_BIG_PERCENT       (20)
#define RR_DIFF_VERY_BIG_PERCENT      (15)
#define RR_DIFF_NOT_BIG_PERCENT       (10)

#define AVER_RR_RANGE                 (8)
#define DEFAULT_EXPECTED_RR           (800)                   /*i.e. 75 bpm*/

#define SI_BIN_WIDTH                  (50)                    /*unit : ms*/
#define SI_BIN_MIN                    (300)                   /*unit : ms*/
#define SI_BIN_MAX                    (1500)                  /*unit : ms*/

#define DETREND_ORDER                 (2)


#define DFA_TREND_ORDER               (3)
#define DFA_S_MIN_ALL                 (5)
#define DFA_S_MAX_ALL                 (50)

#define DFA_S_MIN1                    (5)
#define DFA_S_MAX1                    (15)

#define DFA_S_MIN2                    (16)
#define DFA_S_MAX2                    (50)

#define FFT_RESAMPLE_FREQ_HZ          (3)
#define FFT_DATA_LEN                  (SLIDING_WND * FFT_RESAMPLE_FREQ_HZ)
#define FFT_TOTAL_LEN                 (1024)
#define FFT_ULF_MAX_FREQ_HZ           (0.0033f)
#define FFT_VLF_MAX_FREQ_HZ           (0.04f)
#define FFT_LF_MAX_FREQ_HZ            (0.15f)
#define FFT_HF_MAX_FREQ_HZ            (0.4f)
#define FFT_ULF_END_BIN_IDX           ((uint16_t)ceilf(FFT_ULF_MAX_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))
#define FFT_VLF_END_BIN_IDX           ((uint16_t)ceilf(FFT_VLF_MAX_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))
#define FFT_LF_END_BIN_IDX            ((uint16_t)ceilf(FFT_LF_MAX_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))
#define FFT_HF_END_BIN_IDX            ((uint16_t)ceilf(FFT_HF_MAX_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))

#define RESP_RATE_MIN                 (6)
#define RESP_RATE_MAX                 (30)
#define FFT_RESP_MIN_FREQ_HZ          (1.0f * RESP_RATE_MIN / SEC_PER_MIN)
#define FFT_RESP_MAX_FREQ_HZ          (1.0f * RESP_RATE_MAX / SEC_PER_MIN)
#define FFT_RESP_START_BIN_IDX        ((uint16_t)(FFT_RESP_MIN_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))
#define FFT_RESP_END_BIN_IDX          ((uint16_t)ceilf(FFT_RESP_MAX_FREQ_HZ * FFT_TOTAL_LEN / FFT_RESAMPLE_FREQ_HZ))

typedef struct {
  int time;
  uint8_t hr;
  int16_t rr;
} hr_record_t;

typedef struct {
  int time;
  uint8_t hr;
  int16_t rr;
  int16_t blk_num;
  int16_t expected_rr;
  int16_t correct_rr;
  int16_t rr_trend;
  float relative_time;
} valid_hr_record_t;

#endif
