#pragma once
#ifndef RESPIRATION_FUNCTIONS_H
#define RESPIRATION_FUNCTIONS_H

#include <stdint.h>
#if defined(PC_VERSION)
#define WBD_U8		uint8_t
#define WBD_U16		uint16_t
#define WBD_U32		uint32_t
#define WBD_U64		uint64_t

#define WBD_S8		int8_t
#define WBD_S16		int16_t
#define WBD_S32		int32_t
#define WBD_S64		int64_t

#define WBD_BOOL	uint8_t

#define WBD_FALSE	0
#define WBD_TRUE	(!WBD_FALSE)

#define WBD_F64		double
#define WBD_F32		float
#endif /* #if defined(PC_VERSION) */

#define SUPPORT_INPUT_WITH_HB_TIMESTAMP   (1)

typedef struct {
	uint8_t respiratory_rate;			// unit: breaths per minute (0 means initializing)
	float respiration_current_depth;	// 0 - completely expirated, 1 - completely inspirated, -1 - initializing (NOT meaningful for now)
	uint8_t is_inspirating;				// whether the user is inspirating or outspirating (reserved functionality)
} respiration_result_t;

void init_respiration_analysis(void);
void analyze_respiration(float this_rri);
#if (SUPPORT_INPUT_WITH_HB_TIMESTAMP==1)
void analyze_respiration_with_timestamp(int32_t time, float this_rri); /* time : unit 1/IR_FS, i.e. 8ms */
#endif /* #if (SUPPORT_INPUT_WITH_HB_TIMESTAMP==1) */
respiration_result_t get_respiration_result(void);

#endif // !RESPIRATION_FUNCTIONS_H
