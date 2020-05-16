extern "C" {
#include "main.h"
#include <math.h>
}

#include "ClockPID.h"
#include "platform-clock.h"

double ClockPID_c::average(int32_t *points) {
  double sum = 0;

  for(uint32_t i = 0; i < count; i++) {
    sum += points[i];
  }
  return sum / count;
}

double ClockPID_c::average(uint32_t *points) {
  double sum = 0;

  for(uint32_t i = 0; i < count; i++) {
    sum += points[i];
  }
  return sum / count;
}

static void swap_float(float v[], int i1, int i2) {
  float tmp;

  tmp = v[i1];
  v[i1] = v[i2];
  v[i2] = tmp;
}

static void qsort_float(float v[], int left, int right) {
  int last;

  if (left >= right)
    return;

  swap_float(v, left, (left + right)/2);

  last = left;
  for (int i = left+1; i <= right; i++)
    if (v[i] < v[left])
      swap_float(v, ++last, i);
  swap_float(v, left, last);
  qsort_float(v, left, last-1);
  qsort_float(v, last+1, right);
}

struct linear_result ClockPID_c::theil_sen(float avg_ts, float avg_out) {
  struct linear_result r;
  float slopes[(NTPPID_MAX_COUNT-1)*NTPPID_MAX_COUNT/2];
  uint32_t slopes_count = 0;
  uint32_t median_slope_idx;

  for(uint32_t i = 0; i < (count - 1); i++) {
    for(uint32_t j = i+1; j < count; j++) {
      uint32_t localDuration = timestamps[j] - timestamps[i];
      slopes[slopes_count] = (rawOffsets[j] - rawOffsets[i]) / (float)(localDuration);
      slopes_count++;
    }
  }

  if(slopes_count == 0) {
    r.a = 0;
    r.b = avg_out;
  } else {
    qsort_float(slopes, 0, slopes_count-1);
    median_slope_idx = lround(slopes_count / 2);
    r.a = slopes[median_slope_idx];
    r.b = avg_out - r.a*avg_ts;
  }

  return r;
}

struct linear_result ClockPID_c::simple_linear_regression(double avg_ts, double avg_out, uint32_t *realSecondDurations) {
  struct linear_result r;

  double num = 0, denom = 0;

  for(uint32_t i = 0; i < count; i++) {
    double ts_diff = realSecondDurations[i] - avg_ts;
    num += ts_diff*(double)(rawOffsets[i] - avg_out);
    denom += ts_diff*ts_diff;
  }

  if (denom == 0.0) {
    denom = 1;
  }
  denom *= COUNTSPERSECOND;

  r.a = num / denom;
  r.b = avg_out - (double)r.a * (avg_ts + realSeconds[0]);
  return r;
}

float ClockPID_c::chisq(struct linear_result lin, uint32_t *timestampDurations) {
  float chisq_r = 0;

  for(uint32_t i = 0; i < count; i++) {
    float expected = timestampDurations[i]*lin.a + lin.b;
    chisq_r += pow(rawOffsets[i] - expected, 2.0f);
  }

  return chisq_r;
}

// + for local slower, - for local faster
struct deriv_calc ClockPID_c::calculate_d() {
  double avg_ts, avg_off;
  struct linear_result lin;
  struct deriv_calc d;
  uint32_t timestampDurations[NTPPID_MAX_COUNT];
  uint32_t realSecondDurations[NTPPID_MAX_COUNT];

  rawOffsets[0] = 0;
  timestampDurations[0] = 0;
  realSecondDurations[0] = 0;
  for(uint32_t i = 1; i < count; i++) {
    realSecondDurations[i] = realSeconds[i] - realSeconds[0];
    timestampDurations[i] = timestamps[i] - timestamps[0];
    rawOffsets[i] = realSecondDurations[i] * COUNTSPERSECOND - timestampDurations[i];
  }

  avg_ts = average(realSecondDurations);
  avg_off = average(rawOffsets);

  lin = simple_linear_regression(avg_ts, avg_off, realSecondDurations);
  d.d = lin.a;
  d.d_chisq = chisq(lin, timestampDurations);

  return d;
}

float ClockPID_c::calculate_i() {
  float result = 0;

  for(uint32_t i = 0; i < count; i++) {
    result += corrected_offsets[i];
  }
  return result / 4294967296.0f;
}

// limit to +/- parts (0.000001 = 1ppm)
float ClockPID_c::limit(float factor, float parts) {
  if(factor > parts) {
    return parts;
  } else if(factor < -1*parts) {
    return -1*parts;
  }
  return factor;
}

void ClockPID_c::make_room() {
  for(uint32_t i = 0; i < count-1; i++) {
    timestamps[i] = timestamps[i+1];
    realSeconds[i] = realSeconds[i+1];
    corrected_offsets[i] = corrected_offsets[i+1];
  }

  count--;
}

float ClockPID_c::add_sample(uint32_t timestamp, uint32_t realSecond, int64_t corrected_offset) {
  last_p = corrected_offset / 4294967296.0;

  if(count == NTPPID_MAX_COUNT) {
    make_room();
  }
  timestamps[count] = timestamp;
  realSeconds[count] = realSecond;
  corrected_offsets[count] = corrected_offset;
  count++;

  last_d = calculate_d();
  last_i = calculate_i();

  last_out_p = last_p * kp;
  last_out_i = last_i * ki;
  last_out_d = last_d.d * kd;

  last_out_p = limit(last_out_p, 0.0015);
  last_out_i = limit(last_out_i, 0.000005); // limit I to speed up initial sync
  last_out_d = limit(last_out_d, 0.0015);
  last_out = last_out_p + last_out_i + last_out_d;
  last_out = limit(last_out, 0.0015);

  return last_out;
}
