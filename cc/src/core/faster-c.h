// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// C interface for the C++ code


#ifndef FASTER_C_H_
#define FASTER_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

  typedef struct faster_t faster_t;

  typedef struct faster_result faster_result;
  typedef void (*faster_callback)(faster_result);

  enum faster_status {
      Ok,
      Pending,
      NotFound,
      OutOfMemory,
      IOError,
      Corrupted,
      Aborted
  };
  typedef enum faster_status faster_status;

  typedef void (*read_callback)(void*, uint8_t*, uint64_t, faster_status);

  typedef struct faster_rmw_result faster_rmw_result;
  struct faster_rmw_result {
      uint8_t* value;
      uint64_t size;
  };
  typedef faster_rmw_result (*rmw_callback)(uint8_t*, uint8_t*, uint64_t, uint64_t);

  typedef struct faster_checkpoint_result faster_checkpoint_result;
  struct faster_checkpoint_result {
    bool checked;
    char* token;
  };

  typedef struct faster_recover_result faster_recover_result;
  struct faster_recover_result {
    uint8_t status;
    uint32_t version;
    int session_ids_count;
    char** session_ids;
  };

  // Thread-related operations
  const char* faster_start_session(faster_t* faster_t);
  uint64_t faster_continue_session(faster_t* faster_t, const char* token);
  void faster_stop_session(faster_t* faster_t);
  void faster_refresh_session(faster_t* faster_t);

  // Operations
  faster_t* faster_open_with_disk(const uint64_t table_size, const uint64_t log_size, const char* storage);
  uint8_t faster_upsert(faster_t* faster_t, const uint64_t key, uint8_t* value, uint64_t size);
  uint8_t faster_rmw(faster_t* faster_t, const uint64_t key, uint8_t* value, const uint64_t size, rmw_callback cb);
  uint8_t faster_read(faster_t* faster_t, const uint64_t key, read_callback cb, void* target);
  faster_checkpoint_result* faster_checkpoint(faster_t* faster_t);
  void faster_destroy(faster_t* faster_t);
  uint64_t faster_size(faster_t* faster_t);
  faster_recover_result* faster_recover(faster_t* faster_t, const char* index_token, const char* hybrid_log_token);
  void faster_complete_pending(faster_t* faster_t, bool b);
  // TODO:
  // CheckpointIndex
  // CheckpointHybridLog
  // GrowIndex
  // StartSession
  // StopSession
  // ContinueSession
  // **Statistics**
  // DumpDistribution

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* FASTER_C_H_ */

