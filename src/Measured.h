// Create a struct to hold our persistent settings...
typedef struct persist { // 2 bytes
  uint8_t version;                // version key
  uint8_t inverted;               // Invert display
} __attribute__((__packed__)) persist;

typedef struct persist_debug { // 1 byte
  bool general;              // debugging messages (general)
} __attribute__((__packed__)) persist_debug;

