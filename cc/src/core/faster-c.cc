// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.


#include "faster.h"
#include "faster-c.h"
#include "device/file_system_disk.h"
#include "device/null_disk.h"

extern "C" {

  class Key {
   public:
    Key(uint64_t key)
      : key_{ key } {
    }

    /// Methods and operators required by the (implicit) interface:
    inline static constexpr uint32_t size() {
      return static_cast<uint32_t>(sizeof(Key));
    }
    inline KeyHash GetHash() const {
      return KeyHash{ Utility::GetHashCode(key_) };
    }

    /// Comparison operators.
    inline bool operator==(const Key& other) const {
      return key_ == other.key_;
    }
    inline bool operator!=(const Key& other) const {
      return key_ != other.key_;
    }

   private:
    uint64_t key_;
  };

  class Value {
  public:
      Value()
              : value_ { NULL },
                size_ { 0 } {
      }

      Value(const Value& other)
              : value_{ other.value_ } {
      }

      Value(uint8_t* data, int size) {
        value_ = new uint8_t[size];
        memcpy(value_, data, size);
      }

      inline static constexpr uint32_t size() {
        return static_cast<uint32_t>(sizeof(Value));
      }

      friend class UpsertContext;
      friend class ReadContext;
      friend class RmwContext;

  private:
      union {
          uint8_t* value_;
          std::atomic<uint8_t*> atomic_value_;
      };
      uint64_t size_;
  };

  class ReadContext : public IAsyncContext {
  public:
      typedef Key key_t;
      typedef Value value_t;

      ReadContext(uint64_t key, read_callback cb, void* target)
              : key_{ key }
              , cb_ { cb }
              , target_ { target }  {
      }

      /// Copy (and deep-copy) constructor.
      ReadContext(const ReadContext& other)
              : key_{ other.key_ }
              , cb_ { other.cb_ }
              , target_ { other.target_ }  {
      }

      /// The implicit and explicit interfaces require a key() accessor.
      inline const Key& key() const {
        return key_;
      }

      inline void Get(const value_t& value) {
        cb_(target_, value.value_, value.size_, Ok);
      }
      inline void GetAtomic(const value_t& value) {
        cb_(target_, value.atomic_value_.load(), value.size_, Ok);
      }

      uint64_t val() const {
        return 1;
      }

  protected:
      /// The explicit interface requires a DeepCopy_Internal() implementation.
      Status DeepCopy_Internal(IAsyncContext*& context_copy) {
        return IAsyncContext::DeepCopy_Internal(*this, context_copy);
      }

  private:
      Key key_;
      read_callback cb_;
      void* target_;
  };

  class UpsertContext : public IAsyncContext {
  public:
      typedef Key key_t;
      typedef Value value_t;

      UpsertContext(uint64_t key, uint8_t* input, uint64_t size)
              : key_{ key }
              , input_{ input }
              , size_{ size } {
      }

      /// Copy (and deep-copy) constructor.
      UpsertContext(const UpsertContext& other)
              : key_{ other.key_ }
              , input_{ other.input_ }
              , size_{ other.size_ } {
      }

      /// The implicit and explicit interfaces require a key() accessor.
      inline const Key& key() const {
        return key_;
      }
      inline static constexpr uint32_t value_size() {
        return sizeof(value_t);
      }

      /// Non-atomic and atomic Put() methods.
      /// Note: these are not currently in-place updates
      inline void Put(value_t& value) {
        uint8_t *value_ = new uint8_t[size_];
        memcpy(value_, input_, size_);
        value.value_ = value_;
        value.size_ = size_;
      }
      inline bool PutAtomic(value_t& value) {
        uint8_t *value_ = new uint8_t[size_];
        memcpy(value_, input_, size_);
        value.atomic_value_.store(value_);
        value.size_ = size_;
        return true;
      }

  protected:
      /// The explicit interface requires a DeepCopy_Internal() implementation.
      Status DeepCopy_Internal(IAsyncContext*& context_copy) {
        return IAsyncContext::DeepCopy_Internal(*this, context_copy);
      }

  private:
      Key key_;
      uint8_t* input_;
      uint64_t size_;
  };

  class RmwContext : public IAsyncContext {
  public:
      typedef Key key_t;
      typedef Value value_t;

      RmwContext(uint64_t key, uint8_t* mod, uint64_t size, rmw_callback cb)
              : key_{ key }
              , mod_{ mod }
              , size_{ size }
              , cb_{ cb } {
      }

      /// Copy (and deep-copy) constructor.
      RmwContext(const RmwContext& other)
              : key_{ other.key_ }
              , mod_{ other.mod_ }
              , size_{ other.size_ }
              , cb_{ other.cb_ } {
      }

      /// The implicit and explicit interfaces require a key() accessor.
      const Key& key() const {
        return key_;
      }
      inline static constexpr uint32_t value_size() {
        return sizeof(value_t);
      }

      /// Initial, non-atomic, and atomic RMW methods.
      inline void RmwInitial(value_t& value) {
        uint8_t *value_ = new uint8_t[size_];
        memcpy(value_, mod_, size_);
        value.value_ = value_;
        value.size_ = size_;
      }
      inline void RmwCopy(const value_t& old_value, value_t& value) {
        faster_rmw_result result = cb_(old_value.value_, mod_, old_value.size_, size_);
        uint8_t *value_ = new uint8_t[result.size];
        memcpy(value_, result.value, result.size);
        free(result.value);
        value.value_ = value_;
        value.size_ = result.size;
      }
      inline bool RmwAtomic(value_t& value) {
        uint8_t* current_value = value.atomic_value_.load();
        uint8_t current_size = value.size_;
        faster_rmw_result result = cb_(current_value, mod_, current_size, size_);
        uint8_t *value_ = new uint8_t[result.size];
        memcpy(value_, result.value, result.size);
        free(result.value);
        bool success = value.atomic_value_.compare_exchange_strong(current_value, value_, std::memory_order_release, std::memory_order_relaxed);
        if (success) {
          value.size_ = result.size;
        }
        return success;
      }

  protected:
      /// The explicit interface requires a DeepCopy_Internal() implementation.
      Status DeepCopy_Internal(IAsyncContext*& context_copy) {
        return IAsyncContext::DeepCopy_Internal(*this, context_copy);
      }

  private:
      Key key_;
      uint8_t* mod_;
      uint64_t size_;
      rmw_callback cb_;
  };

  typedef FASTER::environment::QueueIoHandler handler_t;
  typedef FASTER::device::FileSystemDisk<handler_t, 1073741824L> disk_t;
  typedef FASTER::device::NullDisk  disk_null_t;
  using store_t = FasterKv<Key, Value, disk_t>;
  struct faster_t { store_t* obj; };
  
  faster_t* faster_open_with_disk(const uint64_t table_size, const uint64_t log_size, const char* storage) {
    faster_t* res = new faster_t();
    std::experimental::filesystem::create_directory(storage);
    res->obj= new store_t { table_size, log_size, storage };
    return res;
  }

  uint8_t faster_upsert(faster_t* faster_t, const uint64_t key, uint8_t* value, uint64_t size) {
    store_t* store = faster_t->obj;

    auto callback = [](IAsyncContext* ctxt, Status result) {
        assert(result == Status::Ok);
    };

    UpsertContext context { key, value, size };
    Status result = store->Upsert(context, callback, 1);
    return static_cast<uint8_t>(result);
  }

  uint8_t faster_rmw(faster_t* faster_t, const uint64_t key, uint8_t* value, const uint64_t size, rmw_callback cb) {
    store_t* store = faster_t->obj;

    auto callback = [](IAsyncContext* ctxt, Status result) {
        CallbackContext<RmwContext> context { ctxt };
    };

    RmwContext context{ key, value, size, cb};
    Status result = store->Rmw(context, callback, 1);
    return static_cast<uint8_t>(result);
  }

  uint8_t faster_read(faster_t* faster_t, const uint64_t key, read_callback cb, void* target) {
    store_t* store = faster_t->obj;

    auto callback = [](IAsyncContext* ctxt, Status result) {
        CallbackContext<ReadContext> context { ctxt };
    };

    ReadContext context {key, cb, target};
    Status result = store->Read(context, callback, 1);

    if (result == Status::NotFound) {
      cb(target, new uint8_t[0], 0, NotFound);
    }

    return static_cast<uint8_t>(result);
  }

  // It is up to the caller to dealloc faster_checkpoint_result*
  // first token, then struct
  faster_checkpoint_result* faster_checkpoint(faster_t* faster_t) {
    store_t* store = faster_t->obj;
    auto hybrid_log_persistence_callback = [](Status result, uint64_t persistent_serial_num) {
      assert(result == Status::Ok);
    };

    Guid token;
    bool checked = store->Checkpoint(nullptr, hybrid_log_persistence_callback, token);
    //faster_checkpoint_result* res = new faster_checkpoint_result();
    faster_checkpoint_result* res = (faster_checkpoint_result*) malloc(sizeof(faster_checkpoint_result));
    res->checked = checked;
    res->token = (char*) malloc(37 * sizeof(char));
    strncpy(res->token, token.ToString().c_str(), 37);
    return res;
  }

  void faster_destroy(faster_t *faster_t) {
    if (faster_t == NULL)
      return;

    delete faster_t->obj;
    delete faster_t;
  }

  uint64_t faster_size(faster_t* faster_t) {
    if (faster_t == NULL) {
      return -1;
    } else {
      store_t* store = faster_t->obj;
      return store->Size();
    }
  }

  // It is up to the caller to deallocate the faster_recover_result* struct
  faster_recover_result* faster_recover(faster_t* faster_t, const char* index_token, const char* hybrid_log_token) {
    if (faster_t == NULL) {
      return NULL;
    } else {
      store_t* store = faster_t->obj;
      uint32_t ver;
      std::vector<Guid> _session_ids;

      std::string index_str(index_token);
      std::string hybrid_str(hybrid_log_token);
      //TODO: error handling
      Guid index_guid = Guid::Parse(index_str);
      Guid hybrid_guid = Guid::Parse(hybrid_str);
      Status sres = store->Recover(index_guid, hybrid_guid, ver, _session_ids);

      uint8_t status_result = static_cast<uint8_t>(sres);
      faster_recover_result* res = (faster_recover_result*) malloc(sizeof(faster_recover_result));
      res->status= status_result;
      res->version = ver;
    
      int ids_total = _session_ids.size();
      res->session_ids_count = ids_total;
      int session_len = 37; // 36 + 1
      res->session_ids = (char**) malloc(sizeof(char*));

      for (int i = 0; i < ids_total; i++) {
          res->session_ids[i] = (char*) malloc(session_len);
      }

      int counter = 0;
      for (auto& id : _session_ids) {
        strncpy(res->session_ids[counter], id.ToString().c_str(), session_len);
        counter++;
      }

      return res;
    }
  }

  void faster_complete_pending(faster_t* faster_t, bool b) {
    if (faster_t != NULL) {
      store_t* store = faster_t->obj;
      store->CompletePending(b);
    }
  }

  // Thread-related
  
  const char* faster_start_session(faster_t* faster_t) {
    if (faster_t == NULL) {
      return NULL;
    } else {
      store_t* store = faster_t->obj;
      Guid guid = store->StartSession();
      char* str = new char[37];
      std::strcpy(str, guid.ToString().c_str());
      return str;
    }

  }

  uint64_t faster_continue_session(faster_t* faster_t, const char* token) {
    if (faster_t == NULL) {
      return -1;
    } else {
      store_t* store = faster_t->obj;
      std::string guid_str(token);
      Guid guid = Guid::Parse(guid_str);
      return store->ContinueSession(guid);
    }
  }

  void faster_stop_session(faster_t* faster_t) {
    if (faster_t != NULL) {
      store_t* store = faster_t->obj;
      store->StopSession();
    }
  }

  void faster_refresh_session(faster_t* faster_t) {
    if (faster_t != NULL) {
      store_t* store = faster_t->obj;
      store->Refresh();
    }
  }

} // extern "C"
