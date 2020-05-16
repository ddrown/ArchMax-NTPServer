#pragma once

#ifndef NTPPID_KP
#define NTPPID_KP 0.7f
#endif

#ifndef NTPPID_KI
#define NTPPID_KI 0.3f
#endif

#ifndef NTPPID_KD
#define NTPPID_KD 1.0f
#endif

// 51 seconds between counter wraps at 84MHz
#define NTPPID_MAX_COUNT 16

extern "C" {
  struct deriv_calc {
    float d, d_chisq;
  };

  struct linear_result {
    float a, b;
  };
}

class ClockPID_c {
  public:
    ClockPID_c(): count(0), kp(NTPPID_KP), ki(NTPPID_KI), kd(NTPPID_KD) { }
    float add_sample(uint32_t timestamp, uint32_t realSecond, int64_t corrected_offset);

    float p() { return last_p; };
    float i() { return last_i; };
    float d() { return last_d.d; };
    float d_chi() { return last_d.d_chisq; };
    float p_out() { return last_out_p; };
    float i_out() { return last_out_i; };
    float d_out() { return last_out_d; };
    float out() { return last_out; };
    uint32_t samples() { return count; };
    bool full() { return count == NTPPID_MAX_COUNT; };
    void set_kp(float newkp) { kp = newkp; };
    void set_ki(float newki) { ki = newki; };
    void set_kd(float newkd) { kd = newkd; };
    float get_kp() { return kp; };
    float get_ki() { return ki; };
    float get_kd() { return kd; };

    void reset_clock() { count = 0; }

  private:
    uint32_t timestamps[NTPPID_MAX_COUNT]; // in counts
    uint32_t realSeconds[NTPPID_MAX_COUNT]; // actual seconds
    int32_t rawOffsets[NTPPID_MAX_COUNT]; // in counts
    int64_t corrected_offsets[NTPPID_MAX_COUNT]; // in fractional ntp seconds

    uint32_t count;

    float kp, ki, kd;
    float last_p, last_i;
    struct deriv_calc last_d;
    float last_out_p, last_out_i, last_out_d, last_out;

    struct linear_result theil_sen(float avg_ts, float avg_out);
    struct linear_result simple_linear_regression(double avg_ts, double avg_out, uint32_t *realSecondDurations);
    float chisq(struct linear_result lin, uint32_t *timestampDurations);
    struct deriv_calc calculate_d();
    float calculate_i();
    float limit(float factor, float parts);
    void make_room();
    double average(int32_t *points);
    double average(uint32_t *points);
};

extern ClockPID_c ClockPID;
