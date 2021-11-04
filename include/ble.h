#ifndef _ble_h__
#define _ble_h__

#include <math.h>
#include <stdint.h>

#include <vector>

namespace Ble {

/// Predefined data element types
// typedef enum {
//   FILLING_LEVEL_RAW = 1,
//   FILLING_LEVEL,
//   BATTERY_STATUS_RAW,
//   BATTERY_STATUS
// } dataElementType_t;

/// Data element value type (union)
// typedef union value {
//   uint32_t ui32;
//   int32_t i32;
//   float_t f32;
//   value(uint32_t v) : ui32(v) {}
//   value(int32_t v) : i32(v) {}
//   value(float_t v) : f32(v) {}
// } DataElement;

/**
 * Data element implementation
 * 
 * Each data element has a (predefined) type (1 byte) and pointer that points to the element's value (4 bytes).
 * The value must be 4 bytes and can be either uint32_t, int32_t or float_t. In sum each element is 5 bytes long.
 */
class DataElement {
 public:
  DataElement(const void *, size_t);
  DataElement(uint32_t);
  DataElement(int32_t);
  DataElement(float_t);

  size_t valueSize;
  void *value;
};

// typedef std::vector<DataElement> DataElements;

typedef std::vector<DataElement> DataElements;

size_t getSize(DataElements);
void *serializeToRawAdvData(DataElements);

bool advertise(DataElements dataElements);

}  // namespace Ble

#endif