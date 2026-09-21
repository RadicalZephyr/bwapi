/* Toy ABI for the 2026-09-21 note: 1,700 records of 60 int32 + 3 double (264 bytes, the size
 * class of BWAPI::UnitData), read three ways from Python. Not BWAPI code.
 *
 * Usage:  clang -O2 -shared -fPIC -o libabi.so abi.c && python3 bench.py
 *         (numpy is optional; without it the last row prints "(no numpy)")
 */
#include <stdint.h>
#include <string.h>
#define N 1700
typedef struct { int32_t f[60]; double d[3]; } unit_t;   /* 264 bytes, ~UnitData's size class */
static unit_t units[N];
void  init(void){ for(int i=0;i<N;i++){ for(int k=0;k<60;k++) units[i].f[k]=i*60+k; } }
int32_t unit_get(int32_t id, int32_t field){ if(id<0||id>=N) return -1; return units[id].f[field]; }
int32_t unit_get_hp(int32_t id){ return unit_get(id,0); }
int32_t snapshot(unit_t* out, int32_t cap, int32_t alive){ int32_t n = alive<cap?alive:cap; memcpy(out, units, (size_t)n*sizeof(unit_t)); return alive; }
const void* map_base(void){ return units; }
