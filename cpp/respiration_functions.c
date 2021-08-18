#include <string.h>

#include "respiration_functions.h"
#include "polyfit.h"

#define WBD_RRI_BUFFER_SIZE	30
#define WBD_RRI_WINDOW_SIZE	WBD_RRI_BUFFER_SIZE
#define WBD_RRI_FFT_SIZE	128
#define WBD_DETREND_ORDER	2
#define WBD_SEC_PER_MIN		60
#define WBD_MS_PER_SEC		1000
#define WBD_RR_BUFFER_SIZE	30
#define WBD_BOOL uint8_t
#define WBD_S32 int32_t
#define WBD_F32 float
#define WBD_FALSE 0
#define WBD_TRUE 1

static float rri_buffer[WBD_RRI_BUFFER_SIZE];
#if defined(WIN32) && defined(_DEBUG)
uint8_t rri_buffer_count;
#else
static uint8_t rri_buffer_count;
#endif /* #if defined(WIN32) && defined(_DEBUG) */
static respiration_result_t respiration_result;
static float detrend_x_array[WBD_RRI_WINDOW_SIZE];
static float detrended_rri_array[WBD_RRI_WINDOW_SIZE];
static uint8_t respiratory_rate_buffer[WBD_RR_BUFFER_SIZE];
static uint8_t respiratory_rate_buffer_count;

static float roundf(float x)
{
	if (x < 0.0f)
	{
		return 1.0f * (int)(x - 0.5f);
	}
	else
	{
		return 1.0f * (int)(x + 0.5f);
	}
}

static void init_respiration_result()
{
	respiration_result.respiratory_rate = 0;
	respiration_result.respiration_current_depth = -1.0f;
	respiration_result.is_inspirating = WBD_FALSE;
}

static void detrend_rri()
{
	float a[WBD_DETREND_ORDER + 1], trend[WBD_RRI_WINDOW_SIZE];
  uint8_t i;

	if (rri_buffer_count < WBD_RRI_BUFFER_SIZE)
	{
		return;
	}
	polyfit(detrend_x_array, rri_buffer, a, WBD_RRI_WINDOW_SIZE, WBD_DETREND_ORDER);
	polyvals(detrend_x_array, trend, a, WBD_RRI_WINDOW_SIZE, WBD_DETREND_ORDER);
	for (i = 0; i < WBD_RRI_WINDOW_SIZE; i++)
	{
		detrended_rri_array[i] = rri_buffer[i] - trend[i];
	}
}

static void bubble_sort(uint8_t iarr[], uint8_t num)
{
	uint8_t i, j, temp;

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

void init_respiration_analysis()
{
  uint8_t i;
	memset(rri_buffer, 0, WBD_RRI_BUFFER_SIZE * sizeof(float));
	rri_buffer_count = 0;
	init_respiration_result();
	for (i = 0; i < WBD_RRI_WINDOW_SIZE; i++)
	{
		detrend_x_array[i] = (float)i;
	}
	memset(detrended_rri_array, 0, WBD_RRI_WINDOW_SIZE * sizeof(float));
	memset(respiratory_rate_buffer, 0, WBD_RR_BUFFER_SIZE * sizeof(uint8_t));
	respiratory_rate_buffer_count = 0;
}

#if (SUPPORT_INPUT_WITH_HB_TIMESTAMP==1)
#ifndef MS_PER_SECOND
#define MS_PER_SECOND (1000)
#endif

#ifndef IR_FS
#define IR_FS   (125)
#endif

#ifndef MS_PER_SAMPLE
#define MS_PER_SAMPLE (MS_PER_SECOND / IR_FS)
#endif

/* time_index points to the instant of the beat just before this RRI*/
/* time_index is expected to be equal to(or very similar) last_index + last_rri if no missing interval */
/* if only 1 heart beat was rejected : it is expected that 2 intervals were missed : time_index should be ~last_index + 3*last_rri */
/* if more than 1 heart beats were rejected : it is expected that time_index - last_index should be at least ~4*last_rri, we set that threshold to 3.5 to clear the buffer */
#define RRI_DROP_2_BEAT_MULTIPLER_THRESHOLD     (3.5F)
/* if 1 heart beat was rejected : it is expected that time_index - last_index  ~3*last_rri, we set that threshold to 2.5 to insert 2 fake RRI */
#define RRI_DROP_1_BEAT_MULTIPLER_THRESHOLD     (2.5F)
/* if 1 RRI was rejected : it is expected that time_index - last_index  ~2*last_rri, we set that threshold to 1.8 to insert 1 fake RRI */
#define RRI_DROP_1_RRI_MULTIPLER_THRESHOLD      (1.8F)

void analyze_respiration_with_timestamp(WBD_S32 time_index, float this_rri) /* time_index : unit 1/IR_FS, i.e. 8ms */
{
  static WBD_S32 last_index = 0;
  static WBD_F32 last_rri = 800;
  WBD_BOOL fill_rri = 0;

	if (this_rri <= 0.0f)
	{
    last_index = 0;
		return;
	}

  if(last_index>0 && (time_index - last_index)*MS_PER_SAMPLE > (WBD_S32)(last_rri * RRI_DROP_2_BEAT_MULTIPLER_THRESHOLD))
  {
    rri_buffer_count = 0; /* clear RRI buffer */
  }
  else if(last_index>0 && (time_index - last_index)*MS_PER_SAMPLE > (WBD_S32)(last_rri * RRI_DROP_1_BEAT_MULTIPLER_THRESHOLD)) /* this means (2.5 ~ 3.5 * last_rri), i.e. 1 beat missed or 2 intervals missed */
  {
    fill_rri = 2; /* insert 2 fake RRIs */
  }
  else if(last_index>0 && (time_index - last_index)*MS_PER_SAMPLE > (WBD_S32)(last_rri * RRI_DROP_1_RRI_MULTIPLER_THRESHOLD)) /* this mean ~2.0 * last_rri(1.8 < delta < 2.5), quite unlike to have adjacent RRI so different */
  {
    fill_rri = 1; /* insert 1 fake RRI */
  }

	if (rri_buffer_count < WBD_RRI_BUFFER_SIZE)
	{
    if(fill_rri==2)
    {
      if(rri_buffer_count > WBD_RRI_BUFFER_SIZE-3)
      {
        WBD_S32 i, drop_cnt = rri_buffer_count - (WBD_RRI_BUFFER_SIZE-3);
        WBD_F32 estimated_rri;
        estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri) * 0.5F; /* since time delta should be ~3*last_rri */
		    for (i = 0; i < WBD_RRI_BUFFER_SIZE-3; i++)
		    {
			    rri_buffer[i] = rri_buffer[i + drop_cnt];
		    }
        rri_buffer[i] = estimated_rri; i++;
        rri_buffer[i] = estimated_rri; i++;
        rri_buffer[i] = this_rri;
        rri_buffer_count = WBD_RRI_BUFFER_SIZE;
      }
      else
      {
        WBD_F32 estimated_rri;
        estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri) * 0.5F; /* since time delta should be ~3*last_rri */
        rri_buffer[rri_buffer_count++] = estimated_rri;
        rri_buffer[rri_buffer_count++] = estimated_rri;
        rri_buffer[rri_buffer_count++] = this_rri;
      }
    }
    else if(fill_rri==1)
    {
      if(rri_buffer_count > WBD_RRI_BUFFER_SIZE-2)
      {
        WBD_S32 i;
        WBD_F32 estimated_rri;
        estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri); /* since time delta should be ~2*last_rri */
		    for (i = 0; i < WBD_RRI_BUFFER_SIZE-3; i++)
		    {
			    rri_buffer[i] = rri_buffer[i + 1];
		    }
        rri_buffer[i] = estimated_rri; i++;
        rri_buffer[i] = this_rri;
        rri_buffer_count = WBD_RRI_BUFFER_SIZE;
      }
      else
      {
        WBD_F32 estimated_rri;
        estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri); /* since time delta should be ~2*last_rri */
        rri_buffer[rri_buffer_count++] = estimated_rri;
        rri_buffer[rri_buffer_count++] = this_rri;
      }
    }
    else
    {
		  rri_buffer[rri_buffer_count] = this_rri;
		  rri_buffer_count++;
    }
	}
	else
	{
    uint8_t i;
    uint8_t this_zero_crossing_count;
    WBD_BOOL prev_sign;
    float mean_rri;
		float min_rri ;
		float max_rri ;
    uint8_t this_respiratory_rate;
		uint8_t temp_respiratory_rate_buffer[WBD_RR_BUFFER_SIZE];

    if(fill_rri==2) /* insert 2 fake RRIs before this_rri */
    {
      WBD_F32 estimated_rri;
      estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri) * 0.5F; /* since time delta should be ~3*last_rri */
		  for (i = 0; i < rri_buffer_count - 3; i++)
		  {
			  rri_buffer[i] = rri_buffer[i + 3];
		  }
      rri_buffer[i] = estimated_rri; i++;
      rri_buffer[i] = estimated_rri; i++;
      rri_buffer[i] = this_rri;
    }
    if(fill_rri==1)
    {
      WBD_F32 estimated_rri;
      estimated_rri = ((WBD_F32)((time_index - last_index)*MS_PER_SAMPLE) - last_rri); /* since time delta should be ~2*last_rri */
		  for (i = 0; i < rri_buffer_count - 2; i++)
		  {
			  rri_buffer[i] = rri_buffer[i + 2];
		  }
      rri_buffer[i] = estimated_rri; i++;
      rri_buffer[i] = this_rri;
    }
    else
    {
		  for (i = 0; i < rri_buffer_count - 1; i++)
		  {
			  rri_buffer[i] = rri_buffer[i + 1];
		  }
		  rri_buffer[rri_buffer_count - 1] = this_rri;
    }
		detrend_rri();
		this_zero_crossing_count = 0;
		prev_sign = detrended_rri_array[0] >= 0.0f;
		for (i = 1; i < WBD_RRI_WINDOW_SIZE; i++)
		{
			WBD_BOOL this_sign = detrended_rri_array[i] >= 0.0f;
			if (this_sign != prev_sign)
			{
				this_zero_crossing_count++;
			}
			prev_sign = this_sign;
		}
		mean_rri = rri_buffer[0];
		min_rri = rri_buffer[0];
		max_rri = rri_buffer[0];
		for (i = 1; i < WBD_RRI_WINDOW_SIZE; i++)
		{
			float this_iter_rri = rri_buffer[i];
			mean_rri += this_iter_rri;
			if (this_iter_rri < min_rri)
			{
				min_rri = this_iter_rri;
			}
			if (this_iter_rri > max_rri)
			{
				max_rri = this_iter_rri;
			}
		}
		mean_rri /= (float)WBD_RRI_WINDOW_SIZE;
		this_respiratory_rate = (uint8_t)roundf(this_zero_crossing_count / 2.0f * (WBD_SEC_PER_MIN / WBD_RRI_WINDOW_SIZE) * WBD_MS_PER_SEC / mean_rri);
		if (respiratory_rate_buffer_count < WBD_RR_BUFFER_SIZE)
		{
			respiratory_rate_buffer[respiratory_rate_buffer_count] = this_respiratory_rate;
			respiratory_rate_buffer_count++;
		}
		else
		{
			for (i = 0; i < WBD_RR_BUFFER_SIZE - 1; i++)
			{
				respiratory_rate_buffer[i] = respiratory_rate_buffer[i + 1];
			}
			respiratory_rate_buffer[respiratory_rate_buffer_count - 1] = this_respiratory_rate;
		}
    for (i = 0; i < respiratory_rate_buffer_count; i++)
    {
        temp_respiratory_rate_buffer[i] = respiratory_rate_buffer[i];
    }
		bubble_sort(temp_respiratory_rate_buffer, respiratory_rate_buffer_count);
		if (respiratory_rate_buffer_count % 2 != 0)
		{
			respiration_result.respiratory_rate = temp_respiratory_rate_buffer[respiratory_rate_buffer_count / 2];
		}
		else
		{
			respiration_result.respiratory_rate = (uint8_t)roundf(0.5f * (temp_respiratory_rate_buffer[(respiratory_rate_buffer_count - 1) / 2] + temp_respiratory_rate_buffer[respiratory_rate_buffer_count / 2]));
		}
		if (max_rri != min_rri)
		{
			respiration_result.respiration_current_depth = (this_rri - min_rri) / (max_rri - min_rri);
		}
	}
  last_index = time_index;
  last_rri = this_rri;
}
#endif /* #if (SUPPORT_INPUT_WITH_HB_TIMESTAMP==1) */

void analyze_respiration(float this_rri)
{
	if (this_rri <= 0.0f)
	{
		return;
	}
	if (rri_buffer_count < WBD_RRI_BUFFER_SIZE)
	{
		rri_buffer[rri_buffer_count] = this_rri;
		rri_buffer_count++;
	}
	else
	{
    uint8_t i;
    uint8_t this_zero_crossing_count;
    WBD_BOOL prev_sign;
    float mean_rri;
		float min_rri ;
		float max_rri ;
    uint8_t this_respiratory_rate;
		uint8_t temp_respiratory_rate_buffer[WBD_RR_BUFFER_SIZE];

		for (i = 0; i < rri_buffer_count - 1; i++)
		{
			rri_buffer[i] = rri_buffer[i + 1];
		}
		rri_buffer[rri_buffer_count - 1] = this_rri;
		detrend_rri();
		this_zero_crossing_count = 0;
		prev_sign = detrended_rri_array[0] >= 0.0f;
		for (i = 1; i < WBD_RRI_WINDOW_SIZE; i++)
		{
			WBD_BOOL this_sign = detrended_rri_array[i] >= 0.0f;
			if (this_sign != prev_sign)
			{
				this_zero_crossing_count++;
			}
			prev_sign = this_sign;
		}
		mean_rri = rri_buffer[0];
		min_rri = rri_buffer[0];
		max_rri = rri_buffer[0];
		for (i = 1; i < WBD_RRI_WINDOW_SIZE; i++)
		{
			float this_iter_rri = rri_buffer[i];
			mean_rri += this_iter_rri;
			if (this_iter_rri < min_rri)
			{
				min_rri = this_iter_rri;
			}
			if (this_iter_rri > max_rri)
			{
				max_rri = this_iter_rri;
			}
		}
		mean_rri /= (float)WBD_RRI_WINDOW_SIZE;
		this_respiratory_rate = (uint8_t)roundf(this_zero_crossing_count / 2.0f * (WBD_SEC_PER_MIN / WBD_RRI_WINDOW_SIZE) * WBD_MS_PER_SEC / mean_rri);
		if (respiratory_rate_buffer_count < WBD_RR_BUFFER_SIZE)
		{
			respiratory_rate_buffer[respiratory_rate_buffer_count] = this_respiratory_rate;
			respiratory_rate_buffer_count++;
		}
		else
		{
			for (i = 0; i < WBD_RR_BUFFER_SIZE - 1; i++)
			{
				respiratory_rate_buffer[i] = respiratory_rate_buffer[i + 1];
			}
			respiratory_rate_buffer[respiratory_rate_buffer_count - 1] = this_respiratory_rate;
		}
    for (i = 0; i < respiratory_rate_buffer_count; i++)
    {
        temp_respiratory_rate_buffer[i] = respiratory_rate_buffer[i];
    }
		bubble_sort(temp_respiratory_rate_buffer, respiratory_rate_buffer_count);
		if (respiratory_rate_buffer_count % 2 != 0)
		{
			respiration_result.respiratory_rate = temp_respiratory_rate_buffer[respiratory_rate_buffer_count / 2];
		}
		else
		{
			respiration_result.respiratory_rate = (uint8_t)roundf(0.5f * (temp_respiratory_rate_buffer[(respiratory_rate_buffer_count - 1) / 2] + temp_respiratory_rate_buffer[respiratory_rate_buffer_count / 2]));
		}
		if (max_rri != min_rri)
		{
			respiration_result.respiration_current_depth = (this_rri - min_rri) / (max_rri - min_rri);
		}
	}
}

respiration_result_t get_respiration_result()
{
	return respiration_result;
}
