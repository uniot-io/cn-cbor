#ifndef CN_ENCODER_C
#define CN_ENCODER_C

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef EMACS_INDENTATION_HELPER
} /* Duh. */
#endif

#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <assert.h>

#include "include/cn-cbor/cn-cbor.h"
#include "cbor.h"

typedef struct _calc_state
{
  ssize_t offset;
} cn_calc_state;


#define calc_byte_and_data(sz) \
cs->offset++; \
cs->offset += sz;

#define calc_byte() \
cs->offset++;

static uint8_t _xlate[] = {
  IB_FALSE,    /* CN_CBOR_FALSE */
  IB_TRUE,     /* CN_CBOR_TRUE */
  IB_NIL,      /* CN_CBOR_NULL */
  IB_UNDEF,    /* CN_CBOR_UNDEF */
  IB_UNSIGNED, /* CN_CBOR_UINT */
  IB_NEGATIVE, /* CN_CBOR_INT */
  IB_BYTES,    /* CN_CBOR_BYTES */
  IB_TEXT,     /* CN_CBOR_TEXT */
  IB_BYTES,    /* CN_CBOR_BYTES_CHUNKED */
  IB_TEXT,     /* CN_CBOR_TEXT_CHUNKED */
  IB_ARRAY,    /* CN_CBOR_ARRAY */
  IB_MAP,      /* CN_CBOR_MAP */
  IB_TAG,      /* CN_CBOR_TAG */
  IB_PRIM,     /* CN_CBOR_SIMPLE */
  0xFF,        /* CN_CBOR_DOUBLE */
  0xFF         /* CN_CBOR_INVALID */
};

static inline bool is_indefinite(const cn_cbor *cb)
{
  return (cb->flags & CN_CBOR_FL_INDEF) != 0;
}

static void _calc_positive(cn_calc_state *cs, cn_cbor_type typ, uint64_t val) {
  uint8_t ib;

  assert((size_t)typ < sizeof(_xlate));

  ib = _xlate[typ];
  if (ib == 0xFF) {
    cs->offset = -1;
    return;
  }

  if (val < 24) {
    calc_byte();
  } else if (val < 256) {
    calc_byte();
    calc_byte();
  } else if (val < 65536) {
    calc_byte_and_data(2);
  } else if (val < 0x100000000L) {
    calc_byte_and_data(4);
  } else {
    calc_byte_and_data(8);
  }
}

#ifndef CBOR_NO_FLOAT
static void _write_double(cn_calc_state *cs, double val)
{
  float float_val = val;
  if (float_val == val) {                /* 32 bits is enough and we aren't NaN */
    union {
      float f;
      uint32_t u;
    } u32;
    u32.f = float_val;
    if ((u32.u & 0x1FFF) == 0) { /* worth trying half */
      int s16 = (u32.u >> 16) & 0x8000;
      int exp = (u32.u >> 23) & 0xff;
      int mant = u32.u & 0x7fffff;
      if (exp == 0 && mant == 0)
        ;              /* 0.0, -0.0 */
      else if (exp >= 113 && exp <= 142) /* normalized */
        s16 += ((exp - 112) << 10) + (mant >> 13);
      else if (exp >= 103 && exp < 113) { /* denorm, exp16 = 0 */
        if (mant & ((1 << (126 - exp)) - 1))
          goto float32;         /* loss of precision */
        s16 += ((mant + 0x800000) >> (126 - exp));
      } else if (exp == 255 && mant == 0) { /* Inf */
        s16 += 0x7c00;
      } else
        goto float32;           /* loss of range */

      calc_byte_and_data(2);
      return;
    }
  float32:
    calc_byte_and_data(4);
  } else if (val != val) {      /* NaN -- we always write a half NaN*/
    calc_byte_and_data(2);
  } else {
    calc_byte_and_data(8);
  }
}
#endif /* CBOR_NO_FLOAT */

typedef void (*cn_visit_func)(const cn_cbor *cb, int depth, void *context);
static void _visit(const cn_cbor *cb,
                   cn_visit_func visitor,
                   cn_visit_func breaker,
                   void *context)
{
    const cn_cbor *p = cb;
    int depth = 0;
    while (p)
    {
visit:
      visitor(p, depth, context);
      if (p->first_child) {
        p = p->first_child;
        depth++;
      } else{
        // Empty indefinite
        if (is_indefinite(p)) {
          breaker(p->parent, depth, context);
        }
        if (p->next) {
          p = p->next;
        } else {
          while (p->parent) {
            depth--;
            if (is_indefinite(p->parent)) {
              breaker(p->parent, depth, context);
            }
            if (p->parent->next) {
              p = p->parent->next;
              goto visit;
            }
            p = p->parent;
          }
          return;
        }
      }
    }
}

static void _encoder_calculator_visitor(const cn_cbor *cb, int depth, void *context)
{
  cn_calc_state *cs = context;
  UNUSED_PARAM(depth);

  switch (cb->type) {
  case CN_CBOR_ARRAY:
    if (is_indefinite(cb)) {
      calc_byte();
    } else {
      _calc_positive(cs, CN_CBOR_ARRAY, cb->length);
    }
    break;
  case CN_CBOR_MAP:
    if (is_indefinite(cb)) {
      calc_byte();
    } else {
      _calc_positive(cs, CN_CBOR_MAP, cb->length/2);
    }
    break;
  case CN_CBOR_BYTES_CHUNKED:
  case CN_CBOR_TEXT_CHUNKED:
    calc_byte();
    break;

  case CN_CBOR_TEXT:
  case CN_CBOR_BYTES:
    _calc_positive(cs, cb->type, cb->length);
    cs->offset += cb->length;
    break;

  case CN_CBOR_FALSE:
  case CN_CBOR_TRUE:
  case CN_CBOR_NULL:
  case CN_CBOR_UNDEF:
    calc_byte();
    break;

  case CN_CBOR_TAG:
  case CN_CBOR_UINT:
  case CN_CBOR_SIMPLE:
    _calc_positive(cs, cb->type, cb->v.uint);
    break;

  case CN_CBOR_INT:
    assert(cb->v.sint < 0);
    _calc_positive(cs, CN_CBOR_INT, ~(cb->v.sint));
    break;

  case CN_CBOR_DOUBLE:
#ifndef CBOR_NO_FLOAT
    _write_double(cs, cb->v.dbl);
#endif /* CBOR_NO_FLOAT */
    break;

  case CN_CBOR_INVALID:
    cs->offset = -1;
    break;
  }
}

static void _encoder_calculator_breaker(const cn_cbor *cb, int depth, void *context)
{
  cn_calc_state *cs = context;
  UNUSED_PARAM(cb);
  UNUSED_PARAM(depth);
  calc_byte();
}

ssize_t cn_cbor_encoder_calc(const cn_cbor *cb)
{
  cn_calc_state cs = { 0 };
  _visit(cb, _encoder_calculator_visitor, _encoder_calculator_breaker, &cs);
  if (cs.offset < 0) { return -1; }
  return cs.offset + 1;
}

#ifdef  __cplusplus
}
#endif

#endif  /* CN_CBOR_C */
