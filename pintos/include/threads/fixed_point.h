#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

#define F (1<<14) //나중에 곱하거나 나눔

static inline int int_to_fp(int n){
    return n * F;
}

static inline int fp_to_int(int x){
    return x / F;
}

static inline int fp_to_int_round(int x){
    if(x < 0){
        return (x - F/2) / F;
    }
    return (x + F/2 ) / F; // 0.5를 더한 후 나눠서 반올림을 구현한다 
}

static inline int fp_to_int_round_64(int64_t x){
    if(x < 0){
        return (x - F/2) / F;
    }
    return (x + F/2 ) / F;
}

static inline int add_fp(int x1, int x2){
    return x1 + x2;
}

static inline int add_mixed(int x, int n){
    return x + n * F;
}

static inline int sub_fp(int x1, int x2){
    return x1 - x2;
}

static inline int sub_mixed(int x, int n){
    return x - n* F;
}

/*고정 소수점 끼리 곱셈하는 거*/
static inline int mul_fp(int x1, int x2){
    return ( (int64_t)x1 * x2 ) / F;
}

static inline int mul_mixed(int x, int n){
    return ( x * n );
}

static inline int div_fp(int x1, int x2){
    return ((int64_t)x1 * F) / x2;
}

static inline int div_mixed(int x, int n){
    return x/n;
}

#endif
