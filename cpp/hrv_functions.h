#ifndef _HRV_FUNCTIONS_H_
#define _HRV_FUNCTIONS_H_

#include <stdint.h>

#define HRV_ENABLE_STRESS_INDEX
#define HRV_ENABLE_DFA
#define HRV_ENABLE_FFT


typedef struct {
  uint32_t total_rr_cnt;     /* total number of RR intervals received */
  uint16_t window_rr_cnt;    /* number of RR intervals inside the sliding window of previous valid analysis*/
  uint16_t valid_rr_cnt;     /* number of valid RR intervals inside the sliding window of previous valid analysis*/
  int result_timestamp; /* time stamp for last valid analysis result, -1 means no previous valid analysis result*/
  int8_t result_conf_level;/* 0 - 100 */
  int16_t stress_index;     /* stress index : modified algorithm by of a soviet physiologist R.M.Bayevsky*/
  int8_t pNN50;
  float rMSSD;
  float SDNN180;          /*standard deviation of detrended NN over 3 minutes*/
  float HRV_Score;        /* 20 * log rMSSD */
  float dfa_slope1;       /* slope of log10(Fs) vs log10(s) for s =  5-15*/
  float dfa_slope2;       /* slope of log10(Fs) vs log10(s) for s = 16-50*/
  int8_t respiratory_rate; /* average number of respirations per minute, 0 when no output */
  float vlf;           /* absolute power of the very-low-frequency band (0.0033 - 0.04 Hz) in ms^2 */
  float lf;            /* absolute power of the low-frequency band (0.04 - 0.15 Hz) in ms^2 */
  float hf;            /* absolute power of the high-frequency band (0.15 - 0.4 Hz) in ms^2 */
  float lf_nu;         /* normalized power of the low-frequency band (0.04 - 0.15 Hz) in % */
  float hf_nu;         /* normalized power of the high-frequency band (0.15 - 0.4 Hz) in % */
  float lf_to_hf;      /* ratio of LF to HF */
  float total_power;   /* absolute total spectrum power in ms^2 */
} hrv_result_t;

hrv_result_t HRV_Get_Analysis_Result(void);
void init_hrv_analysis(void);
void hrv_analysis(int thistime, uint8_t hr, int16_t rr);

#endif
