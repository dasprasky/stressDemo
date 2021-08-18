#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hrv_functions.h"
#include "hrv_functions_local.h"
#include "polyfit.h"

// #define __PC_DEBUG__

typedef float real;
typedef struct { real Re; real Im; } complex;

#ifndef PI
# define PI	3.14159265358979323846264338327950288f
#endif

static int hrv_start_time;
static hr_record_t hrv_raw_record[RAW_RECORD_LEN];
static uint16_t hrv_raw_record_idx;
static int16_t expected_rr;
static valid_hr_record_t hrv_valid_record[VALID_RECORD_LEN];
static uint16_t rr_record_idx;
#ifdef HRV_ENABLE_DFA
static float x[VALID_RECORD_LEN], y[VALID_RECORD_LEN];
static float Fs2[DFA_S_MAX_ALL+1], Fs[DFA_S_MAX_ALL+1];
#endif


static hrv_result_t hrv_last_analysis_result;
#ifdef __PC_DEBUG__
#define __PC_DEBUG_01__
// #define __PC_DEBUG_02__
#define __PC_DEBUG_03_DFA__
// #define __PC_DEBUG_03A__

static FILE *debugfileptr;
static char *debugfilename = "debug.csv";
#endif

#ifdef HRV_ENABLE_FFT
/*
   https://www.math.wustl.edu/~victor/mfmm/fourier/fft.c
   fft(v,N):
   [0] If N==1 then return.
   [1] For k = 0 to N/2-1, let ve[k] = v[2*k]
   [2] Compute fft(ve, N/2);
   [3] For k = 0 to N/2-1, let vo[k] = v[2*k+1]
   [4] Compute fft(vo, N/2);
   [5] For m = 0 to N/2-1, do [6] through [9]
   [6]   Let w.re = cos(2*PI*m/N)
   [7]   Let w.im = -sin(2*PI*m/N)
   [8]   Let v[m] = ve[m] + w*vo[m]
   [9]   Let v[m+N/2] = ve[m] - w*vo[m]
 */
static void fft(complex* v, uint16_t n, complex* tmp)
{
  if (n > 1)
  {
    /* otherwise, do nothing and return */
    uint16_t k, m;
    complex z, w, * vo, * ve;
    ve = tmp; vo = tmp + n / 2;
    for (k = 0; k < n / 2; k++)
    {
      ve[k] = v[2 * k];
      vo[k] = v[2 * k + 1];
    }
    fft(ve, n / 2, v);		/* FFT on even-indexed elements of v[] */
    fft(vo, n / 2, v);		/* FFT on odd-indexed elements of v[] */
    for (m = 0; m < n / 2; m++)
    {
      w.Re = cosf(2 * PI * m / (float)n);
      w.Im = -sinf(2 * PI * m / (float)n);
      z.Re = w.Re * vo[m].Re - w.Im * vo[m].Im;	/* Re(w*vo[m]) */
      z.Im = w.Re * vo[m].Im + w.Im * vo[m].Re;	/* Im(w*vo[m]) */
      v[m].Re = ve[m].Re + z.Re;
      v[m].Im = ve[m].Im + z.Im;
      v[m + n / 2].Re = ve[m].Re - z.Re;
      v[m + n / 2].Im = ve[m].Im - z.Im;
    }
  }
}

static float even_rr[FFT_TOTAL_LEN] = { 0 };
static complex v[FFT_TOTAL_LEN], tmp[FFT_TOTAL_LEN];

static void calculate_hrv_frequency_domain()
{
  float start_time, this_rr_relative_time, prev_rr_relative_time;
  float rr_mean, prev_rr;
  uint16_t this_rr_idx;
  int16_t even_rr_idx;
  uint16_t bin_idx;
  uint16_t resp_bin_idx;
  float max_bin_amp;
  int rr_even_len;
  int m;


  if (hrv_last_analysis_result.valid_rr_cnt == 0) return;

  /* resample RRs evenly */
  memset(even_rr, 0, FFT_TOTAL_LEN * sizeof(float));
  prev_rr_relative_time = 0.0f;

  this_rr_idx = 0;
  even_rr_idx = 0;

  start_time = hrv_valid_record[this_rr_idx].relative_time;
  prev_rr = hrv_valid_record[this_rr_idx].correct_rr;
  even_rr[even_rr_idx] = prev_rr;
  this_rr_idx++;
  even_rr_idx++;

  for (; this_rr_idx < hrv_last_analysis_result.valid_rr_cnt; this_rr_idx++)
  {
    int16_t this_rr = hrv_valid_record[this_rr_idx].correct_rr;
    float slope;

    this_rr_relative_time = hrv_valid_record[this_rr_idx].relative_time - start_time;
    if (this_rr_relative_time == prev_rr_relative_time)
    {
      prev_rr = this_rr;
      continue;
    }
    slope = (this_rr - prev_rr) / (this_rr_relative_time - prev_rr_relative_time);
    for (; (even_rr_idx <= (int16_t)(this_rr_relative_time * FFT_RESAMPLE_FREQ_HZ)) && (even_rr_idx < FFT_DATA_LEN); even_rr_idx++)
    {
      float this_even_rr_relative_time = (1.0f * even_rr_idx) / (float)FFT_RESAMPLE_FREQ_HZ;
      even_rr[even_rr_idx] = prev_rr + slope * (this_even_rr_relative_time - prev_rr_relative_time);
    }
    prev_rr_relative_time = this_rr_relative_time;
    prev_rr = this_rr;
  }


  rr_mean = 0.0F;
  rr_even_len = 0;
  while(even_rr[rr_even_len]>0) rr_even_len++;

  for(m=0;m<rr_even_len;m++)
  {
    rr_mean += even_rr[m];
  }
  rr_mean = rr_mean / rr_even_len;
  for(m=0;m<rr_even_len;m++)
  {
    even_rr[m] -= rr_mean;
  }
  rr_mean = rr_mean;

  /* perform FFT */
  for (bin_idx = 0; bin_idx < FFT_TOTAL_LEN; bin_idx++)
  {
    v[bin_idx].Re = even_rr[bin_idx];
    v[bin_idx].Im = 0.0f;
  }
  fft(v, FFT_TOTAL_LEN, tmp);
  hrv_last_analysis_result.vlf = 0.0f;
  hrv_last_analysis_result.lf = 0.0f;
  hrv_last_analysis_result.hf = 0.0f;
  hrv_last_analysis_result.total_power = 0.0f;
  resp_bin_idx = 0;
  max_bin_amp = 0.0f;
  for (bin_idx = 0; bin_idx < FFT_TOTAL_LEN / 2; bin_idx++)
  {
    float bin_amp = v[bin_idx].Re * v[bin_idx].Re + v[bin_idx].Im * v[bin_idx].Im;
    hrv_last_analysis_result.total_power += bin_amp;
    if ((bin_idx >= FFT_ULF_END_BIN_IDX) && (bin_idx < FFT_HF_END_BIN_IDX))
    {
      if (bin_idx < FFT_VLF_END_BIN_IDX)
      {
        hrv_last_analysis_result.vlf += bin_amp;
      }
      else if (bin_idx < FFT_LF_END_BIN_IDX)
      {
        hrv_last_analysis_result.lf += bin_amp;
      }
      else
      {
        hrv_last_analysis_result.hf += bin_amp;
      }
    }
    if ((bin_idx > FFT_RESP_START_BIN_IDX) && (bin_idx < FFT_RESP_END_BIN_IDX) && (bin_amp > max_bin_amp))
    {
      max_bin_amp = bin_amp;
      resp_bin_idx = bin_idx;
    }
  }
  hrv_last_analysis_result.vlf /= (FFT_TOTAL_LEN/2 * rr_even_len);
  hrv_last_analysis_result.lf  /= (FFT_TOTAL_LEN/2 * rr_even_len);
  hrv_last_analysis_result.hf  /= (FFT_TOTAL_LEN/2 * rr_even_len);
  hrv_last_analysis_result.lf_nu = 100 * hrv_last_analysis_result.lf / (hrv_last_analysis_result.lf + hrv_last_analysis_result.hf);
  hrv_last_analysis_result.hf_nu = 100 * hrv_last_analysis_result.hf / (hrv_last_analysis_result.lf + hrv_last_analysis_result.hf);
  hrv_last_analysis_result.lf_to_hf = hrv_last_analysis_result.lf / hrv_last_analysis_result.hf;
  hrv_last_analysis_result.total_power /= (FFT_TOTAL_LEN/2 * rr_even_len);
  hrv_last_analysis_result.respiratory_rate = (int8_t)(1.0f * FFT_RESAMPLE_FREQ_HZ * resp_bin_idx / FFT_TOTAL_LEN * SEC_PER_MIN + 0.5f);
}
#endif

#ifdef HRV_ENABLE_DFA
static void calculate_dfa()
{
  uint16_t i, j;
  uint8_t s, block_cnt;
  int32_t sum = 0;
  float xmean;
  float s_array[DFA_S_MAX_ALL+1];
  float Ps[DFA_TREND_ORDER+1], trend[DFA_S_MAX_ALL], X_dt[DFA_S_MAX_ALL];
  float Porder1[2];
  float Porder2[2];
  float log_s[DFA_S_MAX_ALL+1];
  float log_Fs[DFA_S_MAX_ALL+1];
  
  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    sum += hrv_valid_record[i].correct_rr;
  }
  xmean = (float)sum / (float)hrv_last_analysis_result.valid_rr_cnt;
  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    x[i] = (float)hrv_valid_record[i].correct_rr - xmean;
    y[i] = 0;
    for(j=0;j<=i;j++)
    {
      y[i] += x[j];
    }
  }
  for(s=DFA_S_MIN_ALL;s<=DFA_S_MAX_ALL;s++)
  {
    Fs2[s] = Fs[s] = 0.0f;
  }
  for(s=DFA_S_MIN_ALL;s<=DFA_S_MAX_ALL;s++)
  {
    #ifdef __PC_DEBUG_03A__
    debugfileptr = fopen(debugfilename,"wb");
    fclose(debugfileptr);
    #endif
    block_cnt = hrv_last_analysis_result.valid_rr_cnt / s;
    for(j=0;j<block_cnt;j++)
    {
      for(i=0;i<s;i++)
      {
        s_array[i] = i;
      }
      polyfit(s_array, &y[s*j], Ps, s, DFA_TREND_ORDER);
      polyvals(s_array, trend, Ps, s, DFA_TREND_ORDER);
      #ifdef __PC_DEBUG_03A__
      debugfileptr = fopen(debugfilename,"ab");
      for(i=0;i<s;i++)
      {
        fprintf(debugfileptr, "%f, %f, %f, %f\r\n", s_array[i]+s*j, x[s*j+i], y[s*j+i], trend[i]);
      }
      fclose(debugfileptr);
      #endif
      for(i=0;i<s;i++)
      {
        X_dt[i] = y[s*j + i] - trend[i];
        Fs2[s] += (X_dt[i]*X_dt[i]);
      }
    }
    Fs2[s] = Fs2[s] / (s*block_cnt);
    Fs[s] = sqrtf(Fs2[s]);
  }

  for(i=0;i<=DFA_S_MAX_ALL;i++)
  {
    s_array[i] = i;
    log_s[i]  = log10f(s_array[i]);
    log_Fs[i] = log10f(Fs[i]);
  }

  polyfit(&log_s[DFA_S_MIN1], &log_Fs[DFA_S_MIN1], Porder1, DFA_S_MAX1-DFA_S_MIN1+1, 1);
  polyfit(&log_s[DFA_S_MIN2], &log_Fs[DFA_S_MIN2], Porder2, DFA_S_MAX2-DFA_S_MIN2+1, 1);

  hrv_last_analysis_result.dfa_slope1 = Porder1[1];
  hrv_last_analysis_result.dfa_slope2 = Porder2[1];

  #ifdef __PC_DEBUG_03_DFA__
  debugfileptr = fopen("dfa.csv","wb");
  for(s=DFA_S_MIN_ALL;s<=DFA_S_MAX_ALL;s++)
  {
    fprintf(debugfileptr, "%d, %f\r\n", s, Fs[s]);
  }
  fclose(debugfileptr);
  #endif
  s = 0;
}
#endif

static void calculate_hrv_time_domain(void)
{
  uint16_t i, cnt = 0, NN50=0;
  int16_t delta;
  uint32_t sum = 0, rr_sum = 0;
  float rr_mean, sum_square = 0.0f;

  if(hrv_last_analysis_result.valid_rr_cnt > 0)
  {
    for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
    {
      rr_sum += hrv_valid_record[i].correct_rr;
    }
    rr_mean = (float)rr_sum / (float)hrv_last_analysis_result.valid_rr_cnt;
    for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
    {
      sum_square += ((float)hrv_valid_record[i].correct_rr - rr_mean)*((float)hrv_valid_record[i].correct_rr - rr_mean);
    }
    hrv_last_analysis_result.SDNN180 = sqrtf(sum_square / (float)hrv_last_analysis_result.valid_rr_cnt);
  }

  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt-1;i++)
  {
    if(hrv_valid_record[i].blk_num == hrv_valid_record[i+1].blk_num)
    {
      delta = (hrv_valid_record[i].correct_rr - hrv_valid_record[i+1].correct_rr);
      if(abs(delta) > 50) NN50++;

      cnt++;
      sum += (delta*delta);
    }
  }
  if(cnt > 0 && sum > 0)
  {
    hrv_last_analysis_result.pNN50 = 100*NN50/cnt;
    hrv_last_analysis_result.rMSSD = sqrtf((float)sum/(float)cnt);
    hrv_last_analysis_result.HRV_Score = 20.0f * logf(hrv_last_analysis_result.rMSSD);
  }
  else
  {
    hrv_last_analysis_result.pNN50 = 0;
    hrv_last_analysis_result.rMSSD = 0.0f;
    hrv_last_analysis_result.HRV_Score = 0.0f;
  }
}

#ifdef HRV_ENABLE_STRESS_INDEX
static void bubble_sort(int16_t iarr[], uint16_t num)
{
  uint16_t i, j;
  int16_t temp;
 
  for (i = 1; i < num; i++) 
  {
    for (j = 0; j < num - 1; j++) 
    {
      if (iarr[j] > iarr[j + 1]) 
      {
        temp = iarr[j];
        iarr[j] = iarr[j + 1];
        iarr[j + 1] = temp;
      }
    }
  }
}

static void calculate_stress_index(void)
{
  uint16_t i,k,next_i;
  int16_t rr_sorted[VALID_RECORD_LEN];
  int16_t bin_start, bin_end, best_bin_start;
  uint16_t best_bin_cnt;
  float Amo, Mo, MxDMn;

  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    rr_sorted[i] = hrv_valid_record[i].correct_rr;
  }
  bubble_sort(rr_sorted, hrv_last_analysis_result.valid_rr_cnt);
  /*scan for the maximum bin */
  best_bin_start = rr_sorted[0];
  best_bin_cnt = 0;
  k=i=0;
  while(k<hrv_last_analysis_result.valid_rr_cnt)
  {
    bin_start = rr_sorted[i];
    bin_end = bin_start + SI_BIN_WIDTH;
    k = i;
    next_i = 0;
    while(k<hrv_last_analysis_result.valid_rr_cnt)
    {
      if((next_i == 0) && (rr_sorted[k]>rr_sorted[i])) next_i = k;
      if(rr_sorted[k]>=bin_end) break;
      k++;
    }
    if(k-i > best_bin_cnt)
    {
      best_bin_cnt = k-i;
      best_bin_start = bin_start;
    }
    i = next_i;
  }

  Amo = (float)100 * (float)best_bin_cnt / (float)hrv_last_analysis_result.valid_rr_cnt;
  Mo = ((float)best_bin_start + (float)SI_BIN_WIDTH/2) / (float)MS_PER_SEC;
  MxDMn = (float)(rr_sorted[hrv_last_analysis_result.valid_rr_cnt-1] - rr_sorted[0])/(float)MS_PER_SEC;
  hrv_last_analysis_result.stress_index = (int16_t)(0.5f+Amo /((float)2*Mo*MxDMn));
  #ifdef __PC_DEBUG_02__
  printf("Amo : %f, Mo : %f, MxDMn : %f\n", Amo, Mo, MxDMn);
  #endif
}
#endif

static float dt_x[VALID_RECORD_LEN], dt_y[VALID_RECORD_LEN];
static void detrend_rr_intervals(void)
{
  uint16_t i;
  float dt_a[DETREND_ORDER+1];
  float temp_mean;


  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    dt_x[i] = hrv_valid_record[i].relative_time;
    dt_y[i] = (float)hrv_valid_record[i].correct_rr;
  }
  polyfit(dt_x, dt_y, dt_a, hrv_last_analysis_result.valid_rr_cnt, DETREND_ORDER);
  polyvals(dt_x, dt_y, dt_a, hrv_last_analysis_result.valid_rr_cnt, DETREND_ORDER);

  temp_mean = 0.0f;
  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    temp_mean += dt_y[i];
  }
  temp_mean = temp_mean / (float)hrv_last_analysis_result.valid_rr_cnt;

  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    hrv_valid_record[i].rr_trend = (int16_t)dt_y[i];
    hrv_valid_record[i].correct_rr = (int16_t)((float)hrv_valid_record[i].correct_rr - (float)hrv_valid_record[i].rr_trend + temp_mean);
  }
}

static void remove_error_rr_intervals(void)
{
  int16_t i, k, rr_diff_wrong, rr_diff_too_big, rr_diff_very_big, rr_diff_not_big;
  int32_t rr_sum;
  uint16_t rr_cnt;
  float valid_rr_sum = 0.0f, valid_rr_ratio;

  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
  {
    rr_sum = rr_cnt = 0;
    for(k=i-AVER_RR_RANGE;k<=i+AVER_RR_RANGE;k++)
    {
      if((k>=0) && (k<hrv_last_analysis_result.valid_rr_cnt))
      {
        if(hrv_valid_record[k].blk_num == hrv_valid_record[i].blk_num)
        {
          rr_sum += hrv_valid_record[k].rr;
          rr_cnt++;
        }
      }
    }
    if(rr_cnt > AVER_RR_RANGE)
    {
      hrv_valid_record[i].expected_rr = rr_sum / rr_cnt;
    }
    else if(hrv_valid_record[i].hr < UINT8_MAX)
    {
      hrv_valid_record[i].expected_rr = MS_PER_SEC * SEC_PER_MIN / hrv_valid_record[i].hr;
    }
    else
    {
      hrv_valid_record[i].expected_rr = DEFAULT_EXPECTED_RR;
    }
  }
  for(i=0;i<hrv_last_analysis_result.valid_rr_cnt-1;i++)
  {
    if(hrv_valid_record[i].hr < UINT8_MAX)
    {
      expected_rr = hrv_valid_record[i].expected_rr;
      rr_diff_wrong    = expected_rr * RR_DIFF_WRONG_PERCENT    / 100;
      rr_diff_too_big  = expected_rr * RR_DIFF_TOO_BIG_PERCENT  / 100;
      rr_diff_very_big = expected_rr * RR_DIFF_VERY_BIG_PERCENT / 100;
      rr_diff_not_big  = expected_rr * RR_DIFF_NOT_BIG_PERCENT  / 100;

      if(   ( (hrv_valid_record[i].rr   - expected_rr) > rr_diff_very_big                              )  // this rr very big
         && ( (hrv_valid_record[i+1].rr - expected_rr) < (-rr_diff_very_big)                           )  // next rr very small
         && ( abs((hrv_valid_record[i].rr+hrv_valid_record[i+1].rr)/2 - expected_rr) < rr_diff_not_big )  // average of this and next rr looks good
         && ( hrv_valid_record[i].blk_num == hrv_valid_record[i+1].blk_num                             )
        )
      {
        hrv_valid_record[i].correct_rr = (hrv_valid_record[i].rr + hrv_valid_record[i+1].rr)/2;
        hrv_valid_record[i+1].correct_rr = hrv_valid_record[i].correct_rr;
        i++;
      }
      else if(   ((hrv_valid_record[i].rr   - expected_rr) < (-rr_diff_very_big)                             )  // this rr very small
              && ((hrv_valid_record[i+1].rr - expected_rr) > rr_diff_very_big                                )  // next rr very big
              && (abs((hrv_valid_record[i].rr+hrv_valid_record[i+1].rr)/2 - expected_rr) < rr_diff_not_big   )  // average of this and next rr looks good
              && (hrv_valid_record[i].blk_num == hrv_valid_record[i+1].blk_num                               )
           )
      {
        hrv_valid_record[i].correct_rr = (hrv_valid_record[i].rr + hrv_valid_record[i+1].rr)/2;
        hrv_valid_record[i+1].correct_rr = hrv_valid_record[i].correct_rr;
        i++;
      }
      else if(abs(hrv_valid_record[i].rr - expected_rr) > rr_diff_wrong) /*unreasonably big diff from expected*/
      {
        hrv_valid_record[i].correct_rr = expected_rr;
      }
      else if(abs(hrv_valid_record[i].rr - expected_rr) > rr_diff_too_big) /*too big diff from expected*/
      {
        if((i==0) || (hrv_valid_record[i].blk_num != hrv_valid_record[i-1].blk_num)) /* first rr in a block */
        {
          if(abs(hrv_valid_record[i].rr - hrv_valid_record[i+1].rr) > rr_diff_too_big) /*too big diff with next rr*/
          {
            hrv_valid_record[i].correct_rr = expected_rr;
          }
          else
          {
            hrv_valid_record[i].correct_rr = hrv_valid_record[i].rr;
          }
        }
        else /* previous rr belongs to same block*/
        {
          if(abs(hrv_valid_record[i].rr - hrv_valid_record[i-1].correct_rr) > rr_diff_too_big) /*too big diff with previous rr*/
          {
            hrv_valid_record[i].correct_rr = expected_rr;
          }
          else
          {
            hrv_valid_record[i].correct_rr = hrv_valid_record[i].rr;
          }
        }
      }
      else
      {
        hrv_valid_record[i].correct_rr = hrv_valid_record[i].rr;
        valid_rr_sum += (float)hrv_valid_record[i].rr;
      }
    }
  }
  hrv_valid_record[i].correct_rr = hrv_valid_record[i].rr;
  valid_rr_ratio = valid_rr_sum / (float)(SLIDING_WND*1000);
  if(valid_rr_ratio > 1.0f) valid_rr_ratio = 1.0f;
  hrv_last_analysis_result.result_conf_level = (int8_t)((float)100 * valid_rr_ratio + 0.5f);
}

static void crop_rr_intervals(void)
{
  uint16_t idx;
  int wnd_start_time;
  int last_rr_time = -1;
  uint16_t continuous_rr_cnt = 0;
  int16_t block_number = 0;
  int block_start_time = -1;
  int32_t block_accumulated_rr = 0;

  for(rr_record_idx=0;rr_record_idx<VALID_RECORD_LEN;rr_record_idx++)
  {
    hrv_valid_record[rr_record_idx].time    = -1;
    hrv_valid_record[rr_record_idx].hr      = -1;
    hrv_valid_record[rr_record_idx].rr      = -1;
    hrv_valid_record[rr_record_idx].blk_num = -1;
    hrv_valid_record[rr_record_idx].relative_time = -9999.0F;
  }

  rr_record_idx = 0;
  idx = hrv_raw_record_idx;
  wnd_start_time = hrv_raw_record[idx].time - SLIDING_WND;
  idx = (idx<RAW_RECORD_LEN-1)?(idx+1):0;
  hrv_last_analysis_result.window_rr_cnt = 0;

  /*search for sliding window starts*/
  while(1)
  {
    if((hrv_raw_record[idx].time >= wnd_start_time) && (hrv_raw_record[idx].rr > 0))
    {
      hrv_last_analysis_result.window_rr_cnt++;
    }
    if(hrv_raw_record[idx].hr < UINT8_MAX)
    {
      expected_rr = MS_PER_SEC*SEC_PER_MIN/hrv_raw_record[idx].hr;
    }

    if(last_rr_time > 0)
    {
      if(block_start_time < 0)
      {
        block_start_time = hrv_raw_record[idx].time;
        continuous_rr_cnt++;
        block_accumulated_rr = 0;
      }
      else if((block_accumulated_rr + hrv_raw_record[idx].rr) > MS_PER_SEC*(hrv_raw_record[idx].time - block_start_time - 1)) /*no missing RR*/
      {
        continuous_rr_cnt++;
        if(hrv_raw_record[idx].time - block_start_time)
        {
          block_accumulated_rr += hrv_raw_record[idx].rr;
        }
      }
      else
      { 
        if(continuous_rr_cnt >= MIN_CONTINUOUS_RR_CNT)
        {
          block_number++;
        }
        else if(rr_record_idx>=continuous_rr_cnt) /*not enough continuous rr cnt and some record already written to hrv_valid_record[]*/
        {
          rr_record_idx -= continuous_rr_cnt; /*drop this block*/
        }
        block_accumulated_rr = 0;
        block_start_time = hrv_raw_record[idx].time;
        continuous_rr_cnt = 1;
      }
    }
    else
    {
      continuous_rr_cnt = 0;
    }

    if(hrv_raw_record[idx].time >= wnd_start_time)
    {
      hrv_valid_record[rr_record_idx].time    = hrv_raw_record[idx].time - wnd_start_time;
      hrv_valid_record[rr_record_idx].hr      = hrv_raw_record[idx].hr;
      hrv_valid_record[rr_record_idx].rr      = hrv_raw_record[idx].rr;
      hrv_valid_record[rr_record_idx].blk_num = block_number;
      rr_record_idx++;
    }

    if(idx == hrv_raw_record_idx)
    {
      break;
    }

    if(hrv_raw_record[idx].time > 0)
    {
      last_rr_time = hrv_raw_record[idx].time;
    }
    idx = (idx<RAW_RECORD_LEN-1)?(idx+1):0;
  }

  hrv_last_analysis_result.valid_rr_cnt = rr_record_idx;

  {
    uint16_t i;
    int16_t k;
    float temp_time;
    float prev_beat_time;

    prev_beat_time = -9999.0F;

    for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
    {
      if(i==hrv_last_analysis_result.valid_rr_cnt-1 || hrv_valid_record[i+1].time > hrv_valid_record[i].time)
      {
        k = i;
        temp_time = (float)hrv_valid_record[k].time;
        while(k>=0 && hrv_valid_record[k].time == hrv_valid_record[i].time)
        {
          temp_time = temp_time - (float)hrv_valid_record[k].rr / MS_PER_SEC;
          hrv_valid_record[k].relative_time = temp_time;
          k--;
        }
        k++;
        if(prev_beat_time + (float)hrv_valid_record[k].rr / MS_PER_SEC > (hrv_valid_record[k].time-1))
        {
          while(k<=i)
          {
            hrv_valid_record[k].relative_time = prev_beat_time;
            prev_beat_time += (float)hrv_valid_record[k].rr / MS_PER_SEC;
            k++;
          }
        }
        else
        {
          prev_beat_time = (float)hrv_valid_record[i].time;
        }
      }
    }
  }
}

void init_hrv_analysis(void)
{
  uint16_t i;
  hrv_start_time = -1;
  for(i=0;i<RAW_RECORD_LEN;i++)
  {
    hrv_raw_record[i].time = -1;
    hrv_raw_record[i].hr   = UINT8_MAX;
    hrv_raw_record[i].rr   = -1;
  }
  hrv_raw_record_idx = 0;
  expected_rr = -1;
  for(i=0;i<VALID_RECORD_LEN;i++)
  {
    hrv_valid_record[i].time = -1;
    hrv_valid_record[i].hr   = UINT8_MAX;
    hrv_valid_record[i].rr   = -1;
  }
  rr_record_idx = 0;

  hrv_last_analysis_result.result_timestamp  = -1;
  hrv_last_analysis_result.total_rr_cnt      = 0;
  hrv_last_analysis_result.window_rr_cnt     = 0;
  hrv_last_analysis_result.valid_rr_cnt      = 0;
  hrv_last_analysis_result.result_conf_level = -1;
  hrv_last_analysis_result.stress_index      = -1;
  hrv_last_analysis_result.pNN50             = -1;
  hrv_last_analysis_result.rMSSD             = -1.0f;
  hrv_last_analysis_result.SDNN180           = -1.0f;
  hrv_last_analysis_result.HRV_Score         = -1.0f;
  hrv_last_analysis_result.dfa_slope1        = -1.0f;
  hrv_last_analysis_result.dfa_slope2        = -1.0f;
  hrv_last_analysis_result.respiratory_rate  = -1;
  hrv_last_analysis_result.vlf               = -1.0f;
  hrv_last_analysis_result.lf                = -1.0f;
  hrv_last_analysis_result.hf                = -1.0f;
  hrv_last_analysis_result.lf_nu             = -1.0f;
  hrv_last_analysis_result.hf_nu             = -1.0f;
  hrv_last_analysis_result.lf_to_hf          = -1.0f;
  hrv_last_analysis_result.total_power       = -1.0f;
}

void hrv_analysis(int thistime, uint8_t hr, int16_t rr)
{
  if (rr <= 0)
  {
    return;
  }
  hrv_raw_record[hrv_raw_record_idx].time = thistime;
  hrv_raw_record[hrv_raw_record_idx].hr   = hr;
  hrv_raw_record[hrv_raw_record_idx].rr   = rr;

  hrv_last_analysis_result.total_rr_cnt++;

  if(hrv_start_time >= 0)
  {
    if(thistime - hrv_start_time > SLIDING_WND)
    {
      if(thistime - hrv_last_analysis_result.result_timestamp >= ANALYSIS_PERIOD)
      {
        crop_rr_intervals();
        remove_error_rr_intervals();
        detrend_rr_intervals();
        calculate_hrv_time_domain();
#ifdef HRV_ENABLE_STRESS_INDEX
        calculate_stress_index();
#endif
#ifdef HRV_ENABLE_DFA
        calculate_dfa();
#endif
#ifdef HRV_ENABLE_FFT
        calculate_hrv_frequency_domain();
#endif
        hrv_last_analysis_result.result_timestamp = thistime;
        #ifdef __PC_DEBUG__
        {
          int i;

          #ifdef __PC_DEBUG_01__
          debugfileptr = fopen(debugfilename,"wb");
          for(i=0;i<hrv_last_analysis_result.valid_rr_cnt;i++)
          {
            fprintf(debugfileptr, "%4d, %8.3f,%3d,%4d,%d,%4d, %4d, %4d\n", hrv_valid_record[i].time, hrv_valid_record[i].relative_time, hrv_valid_record[i].hr, hrv_valid_record[i].rr, hrv_valid_record[i].blk_num, hrv_valid_record[i].correct_rr, hrv_valid_record[i].expected_rr, hrv_valid_record[i].rr_trend);
          }
          fclose(debugfileptr);
          #endif
        }
        #endif
      }
    }
  }
  else
  {
    hrv_start_time = thistime;
  }

  hrv_raw_record_idx = (hrv_raw_record_idx<RAW_RECORD_LEN-1)?(hrv_raw_record_idx+1):0;
}

hrv_result_t HRV_Get_Analysis_Result(void)
{
  return hrv_last_analysis_result;
}
