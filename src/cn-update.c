#ifndef CN_UPDATE_C
#define CN_UPDATE_C

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>
#include <stdlib.h>

#include "include/cn-cbor/cn-cbor.h"
#include "cbor.h"

bool cn_cbor_string_update(cn_cbor *cb, const char *data)
{
  size_t new_length = strlen(data);

  if (cb->type == CN_CBOR_TEXT)
    if (cb->length == new_length)
      if (memcmp(data, cb->v.str, new_length) == 0)
        return false;

  cb->type = CN_CBOR_TEXT;
  cb->length = new_length;
  cb->v.str = data;

  return true;
}

bool cn_cbor_int_update(cn_cbor *cb, int64_t value)
{
  if (value < 0)
  {
    if (cb->type == CN_CBOR_INT && cb->v.sint == value)
      return false;

    cb->type = CN_CBOR_INT;
    cb->v.sint = value;
  }
  else
  {
    if (cb->type == CN_CBOR_UINT && cb->v.uint == value)
      return false;

    cb->type = CN_CBOR_UINT;
    cb->v.uint = value;
  }

  return true;
}

#ifdef __cplusplus
}
#endif

#endif /* CN_UPDATE_C */